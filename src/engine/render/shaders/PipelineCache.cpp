#include "PipelineCache.h"

#include "d3dx12.h"
#include "ShaderLibrary.h"
#include "engine/rhi/d3d12/D3D12Util.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Render {
	static constexpr std::string_view kChannel = "PipelineCache";

	BuiltInputLayout BuildInputLayout(
		const Rhi::VertexLayoutDesc& layout
	) {
		BuiltInputLayout out;
		out.elements.reserve(layout.elements.size());

		for (const auto& e : layout.elements) {
			D3D12_INPUT_ELEMENT_DESC d = {};
			d.SemanticName             = Rhi::ToD3D12SemanticName(e.semantic);
			d.SemanticIndex            = e.semanticIndex;
			d.Format                   = Rhi::ToDxgiFormat(e.format);
			d.InputSlot                = e.inputSlot;
			d.AlignedByteOffset        = e.offset;
			d.InputSlotClass           = e.perInstance ?
				                   D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA :
				                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
			d.InstanceDataStepRate = e.perInstance ? e.instanceStepRate : 0;

			out.elements.emplace_back(d);
		}

		return out;
	}

	PipelineCache::PipelineCache(
		ID3D12Device* device, ShaderLibrary& shaders
	) : mDevice(device),
	    mShaders(shaders) {
	}

	ID3D12PipelineState* PipelineCache::GetOrCreateGraphicsPso(
		const GraphicsPsoKey& key
	) {
		auto       it           = mGraphics.find(key);
		const bool hasExisting  = it != mGraphics.end();
		const bool needsRebuild =
			!hasExisting || mDirtyGraphics.contains(key);
		if (!needsRebuild) {
			return it->second.Get();
		}
		// ホットリロード失敗時も、既存 PSO があれば描画を継続できるように残す
		mDirtyGraphics.erase(key);

		const auto& vsDxil = mShaders.GetOrCreateDxil(key.vs);
		const auto& psDxil = mShaders.GetOrCreateDxil(key.ps);
		if (vsDxil.bytes.empty() || psDxil.bytes.empty()) {
			Error(
				kChannel,
				"Missing DXIL bytes. VS(id={}, entry='{}') PS(id={}, entry='{}')",
				key.vs.shaderSourceId,
				key.vs.entry,
				key.ps.shaderSourceId,
				key.ps.entry
			);
			if (!hasExisting) {
				mGraphics.emplace(key, nullptr);
				return nullptr;
			}
			return it->second.Get();
		}

		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature                     = key.rootSignature;

		desc.VS = {vsDxil.bytes.data(), vsDxil.bytes.size()};
		desc.PS = {psDxil.bytes.data(), psDxil.bytes.size()};

		desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		desc.BlendState.RenderTarget[0].BlendEnable = key.blendEnable;
		desc.BlendState.RenderTarget[0].SrcBlend = key.srcBlend;
		desc.BlendState.RenderTarget[0].DestBlend = key.destBlend;
		desc.BlendState.RenderTarget[0].BlendOp = key.blendOp;
		desc.BlendState.RenderTarget[0].SrcBlendAlpha = key.srcBlendAlpha;
		desc.BlendState.RenderTarget[0].DestBlendAlpha = key.destBlendAlpha;
		desc.BlendState.RenderTarget[0].BlendOpAlpha = key.blendOpAlpha;
		for (uint32_t i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
			desc.BlendState.RenderTarget[i].RenderTargetWriteMask =
				key.colorWriteMask;
		}
		desc.RasterizerState          = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		desc.RasterizerState.CullMode = key.cullMode;

		desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

		desc.SampleMask = UINT_MAX;

		desc.NumRenderTargets = key.numRenderTargets;
		desc.RTVFormats[0]    = key.numRenderTargets > 0 ?
			                     key.rtvFormat :
			                     DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count   = key.sampleCount;
		desc.SampleDesc.Quality = key.sampleQuality;

		if (key.depthEnable || key.stencilEnable) {
			desc.DSVFormat = key.dsvFormat;
		} else {
			desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		}

		if (key.depthEnable) {
			desc.DepthStencilState.DepthEnable    = TRUE;
			desc.DepthStencilState.DepthWriteMask = key.depthWriteEnable ?
				                                        D3D12_DEPTH_WRITE_MASK_ALL :
				                                        D3D12_DEPTH_WRITE_MASK_ZERO;
			desc.DepthStencilState.DepthFunc = key.depthFunc;
		} else {
			desc.DepthStencilState.DepthEnable = FALSE;
			desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		}

		if (key.stencilEnable) {
			desc.DepthStencilState.StencilEnable    = TRUE;
			desc.DepthStencilState.StencilReadMask  = key.stencilReadMask;
			desc.DepthStencilState.StencilWriteMask = key.stencilWriteMask;

			desc.DepthStencilState.FrontFace.StencilFailOp =
				key.stencilFrontFailOp;
			desc.DepthStencilState.FrontFace.StencilDepthFailOp =
				key.stencilFrontDepthFailOp;
			desc.DepthStencilState.FrontFace.StencilPassOp =
				key.stencilFrontPassOp;
			desc.DepthStencilState.FrontFace.StencilFunc = key.stencilFrontFunc;

			desc.DepthStencilState.BackFace.StencilFailOp =
				key.stencilBackFailOp;
			desc.DepthStencilState.BackFace.StencilDepthFailOp =
				key.stencilBackDepthFailOp;
			desc.DepthStencilState.BackFace.StencilPassOp =
				key.stencilBackPassOp;
			desc.DepthStencilState.BackFace.StencilFunc = key.stencilBackFunc;
		} else {
			desc.DepthStencilState.StencilEnable = FALSE;
		}

		BuiltInputLayout builtInputLayout;
		if (key.vertexLayout.has_value()) {
			builtInputLayout = BuildInputLayout(*key.vertexLayout);
			desc.InputLayout = {
				builtInputLayout.elements.data(),
				static_cast<UINT>(builtInputLayout.elements.size())
			};
		} else {
			desc.InputLayout = {nullptr, 0};
		}

		desc.PrimitiveTopologyType = key.primitiveTopologyType;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		const HRESULT hr = mDevice->CreateGraphicsPipelineState(
			&desc, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf())
		);
		if (FAILED(hr)) {
			Error(
				kChannel,
				"CreateGraphicsPipelineState failed. hr=0x{:08X}",
				static_cast<uint32_t>(hr)
			);
			if (!hasExisting) {
				mGraphics.emplace(key, nullptr);
				return nullptr;
			}
			return it->second.Get();
		}

		if (hasExisting) {
			it->second = std::move(pso);
			return it->second.Get();
		}

		auto* raw = pso.Get();
		mGraphics.emplace(key, std::move(pso));
		return raw;
	}

	ID3D12PipelineState* PipelineCache::GetOrCreateComputePso(
		const ComputePipelineKey& key
	) {
		const auto it           = mCompute.find(key);
		const bool hasExisting  = it != mCompute.end();
		const bool needsRebuild =
			!hasExisting || mDirtyCompute.contains(key);
		if (!needsRebuild) {
			return it->second.Get();
		}
		// ホットリロード失敗時も、既存 PSO があれば描画を継続できるように残す
		mDirtyCompute.erase(key);

		const auto& csDxil = mShaders.GetOrCreateDxil(key.cs);
		if (csDxil.bytes.empty()) {
			Error(
				kChannel,
				"Missing DXIL bytes. CS(id={}, entry='{}')",
				key.cs.shaderSourceId,
				key.cs.entry
			);
			if (!hasExisting) {
				mCompute.emplace(key, nullptr);
				return nullptr;
			}
			return it->second.Get();
		}

		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = key.rootSignature;
		desc.CS = {csDxil.bytes.data(), csDxil.bytes.size()};

		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		const HRESULT hr = mDevice->CreateComputePipelineState(
			&desc, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf())
		);
		if (FAILED(hr)) {
			Error(
				kChannel,
				"CreateComputePipelineState failed. hr=0x{:08X}",
				static_cast<uint32_t>(hr)
			);
			if (!hasExisting) {
				mCompute.emplace(key, nullptr);
				return nullptr;
			}
			return it->second.Get();
		}

		if (hasExisting) {
			it->second = std::move(pso);
			return it->second.Get();
		}

		auto* raw = pso.Get();
		mCompute.emplace(key, std::move(pso));
		return raw;
	}

	void PipelineCache::MarkDirtyByShaderSource(const AssetID shaderSourceId) {
		for (const auto& [key, _] : mGraphics) {
			if (
				key.vs.shaderSourceId == shaderSourceId ||
				key.ps.shaderSourceId == shaderSourceId
			) {
				mDirtyGraphics.emplace(key);
			}
		}

		for (const auto& [key, _] : mCompute) {
			if (key.cs.shaderSourceId == shaderSourceId) {
				mDirtyCompute.emplace(key);
			}
		}
	}

	void PipelineCache::MarkAllDirty() {
		for (const auto& [key, _] : mGraphics) {
			mDirtyGraphics.emplace(key);
		}
		for (const auto& [key, _] : mCompute) {
			mDirtyCompute.emplace(key);
		}
	}

	void PipelineCache::InvalidateAll() {
		mGraphics.clear();
		mCompute.clear();
		mDirtyGraphics.clear();
		mDirtyCompute.clear();
	}
}
