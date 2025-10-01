#include <pch.h>

#include "UPipelineCache.h"

#include "engine/public/urenderer/GraphicsDevice.h"
#include "engine/public/urootsignaturecache/RootSignatureDebugDump.h"

#include "runtime/render/resources/DebugDumpShader.h"

namespace Unnamed {
	static void HashCombine(size_t& h, const size_t v) {
		h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
	}

	static size_t MemHash(const void* p, size_t n) {
		const uint8_t* s = static_cast<const uint8_t*>(p);
		size_t         h = 14695981039346656037ULL;
		for (size_t i = 0; i < n; ++i) {
			h ^= s[i];
			h *= 1099511628211ULL;
		}
		return h;
	}

	UPipelineCache::UPipelineCache(
		GraphicsDevice* graphicsDevice, RootSignatureCache* rootSignatureCache
	) : mGraphicsDevice(graphicsDevice),
	    mRootSignatureCache(rootSignatureCache) {
	}

	PsoHandle UPipelineCache::GetOrCreate(const PipelineDesc& desc) {
		if (!desc.vs || !desc.ps || !desc.vs->blob || !desc.ps->blob) {
			return {};
		}

		const size_t hash = Hash(desc);
		if (const auto it = mHashToIndex.find(hash); it != mHashToIndex.end()) {
			return PsoHandle{it->second};
		}

		std::vector<D3D12_INPUT_ELEMENT_DESC> ild;
		ild.reserve(desc.inputLayout.size());
		for (auto& e : desc.inputLayout) {
			D3D12_INPUT_ELEMENT_DESC ie = {};
			ie.SemanticName = e.semantic;
			ie.SemanticIndex = e.index;
			ie.Format = e.format;
			ie.InputSlot = 0;
			ie.AlignedByteOffset = e.offset;
			ie.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
			ie.InstanceDataStepRate = 0;
			ild.emplace_back(ie);
		}

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
		pso.pRootSignature = mRootSignatureCache->Get(desc.rootSignature);
		pso.VS = {
			.pShaderBytecode = desc.vs->blob->GetBufferPointer(),
			.BytecodeLength = desc.vs->blob->GetBufferSize()
		};
		pso.PS = {
			.pShaderBytecode = desc.ps->blob->GetBufferPointer(),
			.BytecodeLength = desc.ps->blob->GetBufferSize()
		};
		//pso.GS                         = {desc.gs->GetBufferPointer(), desc.gs->GetBufferSize()};
		pso.BlendState            = desc.blend;
		pso.RasterizerState       = desc.rasterizer;
		pso.DepthStencilState     = desc.depth;
		pso.SampleMask            = UINT_MAX;
		pso.InputLayout           = {ild.data(), static_cast<UINT>(ild.size())};
		pso.IBStripCutValue       = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
		pso.PrimitiveTopologyType = desc.topology;
		pso.NumRenderTargets      = desc.numRt;
		for (UINT i = 0; i < desc.numRt; ++i) {
			pso.RTVFormats[i] = desc.rtv[i];
		}
		pso.DSVFormat  = desc.dsv;
		pso.SampleDesc = desc.sample;

#ifdef _DEBUG
		auto& rsDesc = mRootSignatureCache->GetDesc(desc.rootSignature);
		DumpRootSignatureDesc(rsDesc, "RS before PSO");
		DumpShaderResources(pso.VS, "VS");
		ValidateRSvsShader(rsDesc, pso.VS, D3D12_SHADER_VISIBILITY_VERTEX,
		                   "VS");
		if (pso.PS.pShaderBytecode) {
			DumpShaderResources(pso.PS, "PS");
			ValidateRSvsShader(rsDesc, pso.PS, D3D12_SHADER_VISIBILITY_PIXEL,
			                   "PS");
		}
#endif

		Microsoft::WRL::ComPtr<ID3D12PipelineState> psoObj;
		THROW(
			mGraphicsDevice->Device()->CreateGraphicsPipelineState(
				&pso, IID_PPV_ARGS(&psoObj)
			)
		);

		Entry entry    = {};
		entry.hash     = hash;
		entry.pso      = std::move(psoObj);
		uint32_t index = static_cast<uint32_t>(mEntries.size());
		mEntries.emplace_back(std::move(entry));
		mHashToIndex[hash] = index;
		return PsoHandle{index};
	}

	ID3D12PipelineState* UPipelineCache::Get(const PsoHandle handle) const {
		if (handle.id >= mEntries.size()) {
			return nullptr;
		}
		return mEntries[handle.id].pso.Get();
	}

	size_t UPipelineCache::Hash(const PipelineDesc& desc) {
		constexpr std::hash<uint64_t>    hashInt;
		constexpr std::hash<std::string> hashString;
		size_t                           h = hashInt(desc.rootSignature.id);
		for (UINT i = 0; i < desc.numRt; ++i) {
			HashCombine(h, hashInt(static_cast<uint64_t>(desc.rtv[i])));
		}

		HashCombine(h, hashInt(desc.dsv));
		HashCombine(h, hashInt(desc.topology));
		HashCombine(h, hashInt(desc.sample.Count));
		HashCombine(h, hashInt(desc.sample.Quality));

		// State
		HashCombine(h, MemHash(&desc.blend, sizeof(desc.blend)));
		HashCombine(h, MemHash(&desc.rasterizer, sizeof(desc.rasterizer)));
		HashCombine(h, MemHash(&desc.depth, sizeof(desc.depth)));

		// InputLayout
		for (auto& e : desc.inputLayout) {
			HashCombine(h, hashString(e.semantic ? e.semantic : ""));
			HashCombine(h, hashInt(e.index));
			HashCombine(h, hashInt(static_cast<uint64_t>(e.format)));
			HashCombine(h, hashInt(e.offset));
		}

		// Shader
		if (desc.vs && desc.vs->blob) {
			HashCombine(h, hashInt(desc.vs->blob->GetBufferSize()));
			HashCombine(
				h,
				hashInt(
					reinterpret_cast<uintptr_t>(
						desc.vs->blob->GetBufferPointer()
					)
				)
			);
		}
		if (desc.ps && desc.ps->blob) {
			HashCombine(h, hashInt(desc.ps->blob->GetBufferSize()));
			HashCombine(
				h,
				hashInt(
					reinterpret_cast<uintptr_t>(
						desc.ps->blob->GetBufferPointer()
					)
				)
			);
		}

		return h;
	}
}
