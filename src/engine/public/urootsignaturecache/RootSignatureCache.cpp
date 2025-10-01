#include "RootSignatureCache.h"

#include <stdexcept>
#include <string>

#include <engine/public/subsystem/console/Log.h>

#include "RootSignatureDebugDump.h"

#include "engine/public/urenderer/GraphicsDevice.h"

namespace Unnamed {
	constexpr std::string_view kChannel = "RootSignatureCache";

	static void HashCombine(size_t& h, const size_t v) {
		h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
	}

	static D3D12_DESCRIPTOR_RANGE_TYPE ToD3D(const RangeType type) {
		switch (type) {
		case RangeType::SRV: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case RangeType::UAV: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		case RangeType::CBV: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		case RangeType::SAMPLER: return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		default: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		}
	}

	RootSignatureCache::RootSignatureCache(GraphicsDevice* graphicsDevice) :
		mGraphicsDevice(graphicsDevice) {
	}

	RootSignatureHandle RootSignatureCache::GetOrCreate(
		const RootSignatureDesc& desc
	) {
		const size_t hash = Hash(desc);
		if (auto it = mHashToIndex.find(hash); it != mHashToIndex.end()) {
			return RootSignatureHandle{it->second};
		}

		std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> rangesScratch;
		std::vector<D3D12_ROOT_PARAMETER>                rootParams;
		rangesScratch.reserve(desc.params.size());
		rootParams.reserve(desc.params.size());

		for (auto& param : desc.params) {
			D3D12_ROOT_PARAMETER rootParam;
			switch (param.kind) {
			case RootParamDesc::Kind::TABLE: {
				auto& vr = rangesScratch.emplace_back();
				vr.reserve(param.ranges.size());
				for (auto& range : param.ranges) {
					D3D12_DESCRIPTOR_RANGE descRange;
					descRange.RangeType = ToD3D(range.type);
					descRange.NumDescriptors = range.count;
					descRange.BaseShaderRegister = range.baseRegister;
					descRange.RegisterSpace = range.space;
					descRange.OffsetInDescriptorsFromTableStart =
						D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
					vr.push_back(descRange);
				}
				rootParam.ParameterType =
					D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				rootParam.DescriptorTable.NumDescriptorRanges = static_cast<
					UINT>(vr.size());
				rootParam.DescriptorTable.pDescriptorRanges = vr.data();
				rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}
			break;
			case RootParamDesc::Kind::ROOT_CBV: {
				rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				rootParam.Descriptor.ShaderRegister = param.cbvRegister;
				rootParam.Descriptor.RegisterSpace = param.cbvSpace;
				rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}
			break;
			case RootParamDesc::Kind::ROOT32_BIT_CONST: {
				rootParam.ParameterType =
					D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
				rootParam.Constants.Num32BitValues = param.num32Bit;
				rootParam.Constants.ShaderRegister = param.constRegister;
				rootParam.Constants.RegisterSpace = param.constSpace;
				rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}
			break;
			default: ;
			}

			rootParams.emplace_back(rootParam);
		}

		std::vector<D3D12_STATIC_SAMPLER_DESC> smps;
		smps.reserve(desc.staticSamplers.size());
		for (auto& sampler : desc.staticSamplers) {
			D3D12_STATIC_SAMPLER_DESC ss = {};
			ss.Filter = sampler.desc.Filter;
			ss.AddressU = sampler.desc.AddressU;
			ss.AddressV = sampler.desc.AddressV;
			ss.AddressW = sampler.desc.AddressW;
			ss.MipLODBias = sampler.desc.MipLODBias;
			ss.MaxAnisotropy = sampler.desc.MaxAnisotropy;
			ss.ComparisonFunc = sampler.desc.ComparisonFunc;
			ss.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			ss.MinLOD = sampler.desc.MinLOD;
			ss.MaxLOD = sampler.desc.MaxLOD;
			ss.ShaderRegister = sampler.reg;
			ss.RegisterSpace = sampler.space;
			ss.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			smps.emplace_back(ss);
		}

		D3D12_ROOT_SIGNATURE_DESC rs = {};
		rs.Flags                     = desc.flags;
		rs.NumParameters             = static_cast<UINT>(rootParams.size());
		rs.pParameters               = rootParams.data();
		rs.NumStaticSamplers         = static_cast<UINT>(smps.size());
		rs.pStaticSamplers           = smps.data();

#ifdef _DEBUG
		DumpRootSignatureDesc(rs, "UMaterialRuntime RS");
#endif

		Microsoft::WRL::ComPtr<ID3DBlob> blob, error;
		HRESULT                          hr = D3D12SerializeRootSignature(
			&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error
		);

		if (FAILED(hr)) {
			if (error) {
				Fatal(
					kChannel,
					"{}",
					error->GetBufferPointer()
				);
			}
			return {};
		}

		Microsoft::WRL::ComPtr<ID3D12RootSignature> rsObj;
		THROW(
			mGraphicsDevice->Device()->CreateRootSignature(
				0, blob-> GetBufferPointer(),
				blob->GetBufferSize(), IID_PPV_ARGS(&rsObj)
			)
		);

		Entry entry;
		entry.hash              = hash;
		entry.rootSignature     = std::move(rsObj);
		entry.rootSignatureDesc = rs;
		uint32_t index          = static_cast<uint32_t>(mEntries.size());
		mEntries.emplace_back(std::move(entry));
		mHashToIndex[hash] = index;
		return RootSignatureHandle{index};
	}

	ID3D12RootSignature* RootSignatureCache::Get(
		const RootSignatureHandle handle) const {
		if (handle.id >= mEntries.size()) {
			return nullptr;
		}
		return mEntries[handle.id].rootSignature.Get();
	}

	D3D12_ROOT_SIGNATURE_DESC& RootSignatureCache::GetDesc(
		const RootSignatureHandle rootSignature) {
		return mEntries[rootSignature.id].rootSignatureDesc;
	}

	size_t RootSignatureCache::Hash(const RootSignatureDesc& desc) {
		std::hash<uint64_t> hash;
		size_t              h = hash(desc.flags);
		for (auto& param : desc.params) {
			HashCombine(h, hash(static_cast<uint64_t>(param.kind)));
			HashCombine(h, hash(param.cbvRegister));
			HashCombine(h, hash(param.cbvSpace));
			HashCombine(h, hash(param.num32Bit));
			HashCombine(h, hash(param.constRegister));
			HashCombine(h, hash(param.constSpace));
			for (auto& range : param.ranges) {
				HashCombine(h, hash(static_cast<uint64_t>(range.type)));
				HashCombine(h, hash(range.baseRegister));
				HashCombine(h, hash(range.count));
				HashCombine(h, hash(range.space));
			}
		}

		for (auto& sampler : desc.staticSamplers) {
			HashCombine(h, hash(sampler.reg));
			HashCombine(h, hash(sampler.space));
			// sampler desc の主に使用する値
			HashCombine(h, hash(sampler.desc.Filter));
			HashCombine(h, hash(sampler.desc.AddressU));
			HashCombine(h, hash(sampler.desc.AddressV));
			HashCombine(h, hash(sampler.desc.AddressW));
		}
		return h;
	}
}
