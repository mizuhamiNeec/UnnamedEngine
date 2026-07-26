#pragma once
#include "ShaderKey.h"

#include <d3d12.h>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include <wrl/client.h>

#include "core/hash/HashBuilder.h"
#include "engine/rhi/PipelineKey.h"

namespace Unnamed::Render {
	class ShaderLibrary;

	/// @brief GraphicsPsoKeyは、graphics PSOを一意に決めるshader、root signature、raster・depth・blend状態を保持します
	struct GraphicsPsoKey {
		ShaderKey vs;
		ShaderKey ps;

		ID3D12RootSignature*                 rootSignature = nullptr;
		std::optional<Rhi::VertexLayoutDesc> vertexLayout  = std::nullopt;

		uint8_t                       numRenderTargets = 1;
		DXGI_FORMAT                   rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		uint32_t                      sampleCount   = 1;
		uint32_t                      sampleQuality = 0;
		D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType =
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		bool                  depthEnable = false;
		DXGI_FORMAT           dsvFormat   = DXGI_FORMAT_UNKNOWN;
		D3D12_COMPARISON_FUNC depthFunc   = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		bool    stencilEnable    = false;
		uint8_t stencilReadMask  = D3D12_DEFAULT_STENCIL_READ_MASK;
		uint8_t stencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

		D3D12_STENCIL_OP      stencilFrontFailOp = D3D12_STENCIL_OP_KEEP;
		D3D12_STENCIL_OP      stencilFrontDepthFailOp = D3D12_STENCIL_OP_KEEP;
		D3D12_STENCIL_OP      stencilFrontPassOp = D3D12_STENCIL_OP_KEEP;
		D3D12_COMPARISON_FUNC stencilFrontFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		D3D12_STENCIL_OP      stencilBackFailOp = D3D12_STENCIL_OP_KEEP;
		D3D12_STENCIL_OP      stencilBackDepthFailOp = D3D12_STENCIL_OP_KEEP;
		D3D12_STENCIL_OP      stencilBackPassOp = D3D12_STENCIL_OP_KEEP;
		D3D12_COMPARISON_FUNC stencilBackFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		bool            depthWriteEnable = true;
		uint8_t         colorWriteMask   = D3D12_COLOR_WRITE_ENABLE_ALL;
		D3D12_CULL_MODE cullMode         = D3D12_CULL_MODE_BACK;
		bool            blendEnable      = false;
		D3D12_BLEND     srcBlend         = D3D12_BLEND_ONE;
		D3D12_BLEND     destBlend        = D3D12_BLEND_ZERO;
		D3D12_BLEND_OP  blendOp          = D3D12_BLEND_OP_ADD;
		D3D12_BLEND     srcBlendAlpha    = D3D12_BLEND_ONE;
		D3D12_BLEND     destBlendAlpha   = D3D12_BLEND_ZERO;
		D3D12_BLEND_OP  blendOpAlpha     = D3D12_BLEND_OP_ADD;

		bool operator==(const GraphicsPsoKey& rhs) const {
			return vs == rhs.vs &&
			       ps == rhs.ps &&
			       rootSignature == rhs.rootSignature &&
			       numRenderTargets == rhs.numRenderTargets &&
			       rtvFormat == rhs.rtvFormat &&
			       sampleCount == rhs.sampleCount &&
			       sampleQuality == rhs.sampleQuality &&
			       primitiveTopologyType == rhs.primitiveTopologyType &&
			       depthEnable == rhs.depthEnable &&
			       dsvFormat == rhs.dsvFormat &&
			       depthFunc == rhs.depthFunc &&
			       stencilEnable == rhs.stencilEnable &&
			       stencilReadMask == rhs.stencilReadMask &&
			       stencilWriteMask == rhs.stencilWriteMask &&
			       stencilFrontFailOp == rhs.stencilFrontFailOp &&
			       stencilFrontDepthFailOp == rhs.stencilFrontDepthFailOp &&
			       stencilFrontPassOp == rhs.stencilFrontPassOp &&
			       stencilFrontFunc == rhs.stencilFrontFunc &&
			       stencilBackFailOp == rhs.stencilBackFailOp &&
			       stencilBackDepthFailOp == rhs.stencilBackDepthFailOp &&
			       stencilBackPassOp == rhs.stencilBackPassOp &&
			       stencilBackFunc == rhs.stencilBackFunc &&
			       depthWriteEnable == rhs.depthWriteEnable &&
			       colorWriteMask == rhs.colorWriteMask &&
			       cullMode == rhs.cullMode &&
			       blendEnable == rhs.blendEnable &&
			       srcBlend == rhs.srcBlend &&
			       destBlend == rhs.destBlend &&
			       blendOp == rhs.blendOp &&
			       srcBlendAlpha == rhs.srcBlendAlpha &&
			       destBlendAlpha == rhs.destBlendAlpha &&
			       blendOpAlpha == rhs.blendOpAlpha &&
			       vertexLayout == rhs.vertexLayout;
		}
	};

	/// @brief ComputePipelineKeyは、compute PSOを一意に決めるshaderとroot signatureを保持します
	struct ComputePipelineKey {
		ShaderKey            cs;
		ID3D12RootSignature* rootSignature = nullptr;

		bool operator==(const ComputePipelineKey& rhs) const {
			return cs == rhs.cs &&
			       rootSignature == rhs.rootSignature;
		}
	};

	inline size_t HashVertexLayout(const Rhi::VertexLayoutDesc& layout) {
		HashBuilder hash = {};
		hash.AddValue(layout.stride);
		hash.AddValue(layout.elements.size());

		for (const auto& e : layout.elements) {
			hash.AddEnum(e.semantic);
			hash.AddValue(e.semanticIndex);
			hash.AddEnum(e.format);
			hash.AddValue(e.offset);
			hash.AddValue(e.inputSlot);
			hash.AddValue(e.perInstance);
			hash.AddValue(e.instanceStepRate);
		}
		return hash.Value();
	}

	/// @brief GraphicsPipelineKeyHashは、シェーダー、ルートシグネチャー、描画状態、頂点レイアウトからGraphicsPsoKeyのハッシュを計算します
	struct GraphicsPipelineKeyHash {
		size_t operator()(const GraphicsPsoKey& k) const noexcept {
			HashBuilder hash = {};

			hash.AddHashed(ShaderKeyHash{}(k.vs));
			hash.AddHashed(ShaderKeyHash{}(k.ps));
			hash.AddPointer(k.rootSignature);
			hash.AddValue(k.numRenderTargets);
			hash.AddEnum(k.rtvFormat);
			hash.AddValue(k.sampleCount);
			hash.AddValue(k.sampleQuality);
			hash.AddEnum(k.primitiveTopologyType);

			hash.AddValue(k.depthEnable);
			hash.AddEnum(k.dsvFormat);
			hash.AddEnum(k.depthFunc);
			hash.AddValue(k.stencilEnable);
			hash.AddValue(k.stencilReadMask);
			hash.AddValue(k.stencilWriteMask);
			hash.AddEnum(k.stencilFrontFailOp);
			hash.AddEnum(k.stencilFrontDepthFailOp);
			hash.AddEnum(k.stencilFrontPassOp);
			hash.AddEnum(k.stencilFrontFunc);
			hash.AddEnum(k.stencilBackFailOp);
			hash.AddEnum(k.stencilBackDepthFailOp);
			hash.AddEnum(k.stencilBackPassOp);
			hash.AddEnum(k.stencilBackFunc);
			hash.AddValue(k.depthWriteEnable);
			hash.AddValue(k.colorWriteMask);
			hash.AddEnum(k.cullMode);
			hash.AddValue(k.blendEnable);
			hash.AddEnum(k.srcBlend);
			hash.AddEnum(k.destBlend);
			hash.AddEnum(k.blendOp);
			hash.AddEnum(k.srcBlendAlpha);
			hash.AddEnum(k.destBlendAlpha);
			hash.AddEnum(k.blendOpAlpha);

			hash.AddValue(k.vertexLayout.has_value());
			if (k.vertexLayout.has_value()) {
				hash.AddHashed(HashVertexLayout(*k.vertexLayout));
			}

			return hash.Value();
		}
	};

	/// @brief GraphicsPipelineKeyEqualは、GraphicsPsoKeyの全パイプライン状態を比較して同一PSOを表すか判定します
	struct GraphicsPipelineKeyEqual {
		bool operator()(
			const GraphicsPsoKey& a, const GraphicsPsoKey& b
		) const {
			if (a.vs != b.vs) {
				return false;
			}
			if (a.ps != b.ps) {
				return false;
			}
			if (a.rootSignature != b.rootSignature) {
				return false;
			}
			if (a.numRenderTargets != b.numRenderTargets) {
				return false;
			}
			if (a.rtvFormat != b.rtvFormat) {
				return false;
			}
			if (a.sampleCount != b.sampleCount) {
				return false;
			}
			if (a.sampleQuality != b.sampleQuality) {
				return false;
			}
			if (a.primitiveTopologyType != b.primitiveTopologyType) {
				return false;
			}
			if (a.depthEnable != b.depthEnable) {
				return false;
			}
			if (a.dsvFormat != b.dsvFormat) {
				return false;
			}
			if (a.depthFunc != b.depthFunc) {
				return false;
			}
			if (a.stencilEnable != b.stencilEnable) {
				return false;
			}
			if (a.stencilReadMask != b.stencilReadMask) {
				return false;
			}
			if (a.stencilWriteMask != b.stencilWriteMask) {
				return false;
			}
			if (a.stencilFrontFailOp != b.stencilFrontFailOp) {
				return false;
			}
			if (a.stencilFrontDepthFailOp != b.stencilFrontDepthFailOp) {
				return false;
			}
			if (a.stencilFrontPassOp != b.stencilFrontPassOp) {
				return false;
			}
			if (a.stencilFrontFunc != b.stencilFrontFunc) {
				return false;
			}
			if (a.stencilBackFailOp != b.stencilBackFailOp) {
				return false;
			}
			if (a.stencilBackDepthFailOp != b.stencilBackDepthFailOp) {
				return false;
			}
			if (a.stencilBackPassOp != b.stencilBackPassOp) {
				return false;
			}
			if (a.stencilBackFunc != b.stencilBackFunc) {
				return false;
			}
			if (a.depthWriteEnable != b.depthWriteEnable) {
				return false;
			}
			if (a.colorWriteMask != b.colorWriteMask) {
				return false;
			}
			if (a.cullMode != b.cullMode) {
				return false;
			}
			if (a.blendEnable != b.blendEnable) {
				return false;
			}
			if (a.srcBlend != b.srcBlend) {
				return false;
			}
			if (a.destBlend != b.destBlend) {
				return false;
			}
			if (a.blendOp != b.blendOp) {
				return false;
			}
			if (a.srcBlendAlpha != b.srcBlendAlpha) {
				return false;
			}
			if (a.destBlendAlpha != b.destBlendAlpha) {
				return false;
			}
			if (a.blendOpAlpha != b.blendOpAlpha) {
				return false;
			}
			if (a.vertexLayout.has_value() != b.vertexLayout.has_value()) {
				return false;
			}
			if (a.vertexLayout.has_value()) {
				if (*a.vertexLayout != *b.vertexLayout) {
					return false;
				}
			}

			return true;
		}
	};

	/// @brief ComputePipelineKeyHashは、コンピュートシェーダーとルートシグネチャーからComputePipelineKeyのハッシュを計算します
	struct ComputePipelineKeyHash {
		size_t operator()(const ComputePipelineKey& k) const noexcept {
			HashBuilder hash = {};
			hash.AddHashed(ShaderKeyHash{}(k.cs));
			hash.AddPointer(k.rootSignature);
			return hash.Value();
		}
	};

	/// @brief BuiltInputLayoutは、PSO作成まで文字列を生存させながらD3D12入力要素配列を所有します
	struct BuiltInputLayout {
		std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
	};

	BuiltInputLayout BuildInputLayout(
		const Rhi::VertexLayoutDesc& layout
	);

	/// @brief PipelineCacheは、描画・コンピュート設定をキーにD3D12 PSOを生成して再利用します
	class PipelineCache {
	public:
		PipelineCache(ID3D12Device* device, ShaderLibrary& shaders);

		ID3D12PipelineState* GetOrCreateGraphicsPso(
			const GraphicsPsoKey& key
		);
		ID3D12PipelineState* GetOrCreateComputePso(
			const ComputePipelineKey& key
		);

		void MarkDirtyByShaderSource(AssetID shaderSourceId);
		void MarkAllDirty();
		void InvalidateAll();

	private:
		ID3D12Device*  mDevice = nullptr;
		ShaderLibrary& mShaders;

		std::unordered_map<
			GraphicsPsoKey,
			Microsoft::WRL::ComPtr<ID3D12PipelineState>,
			GraphicsPipelineKeyHash
		> mGraphics;
		std::unordered_map<
			ComputePipelineKey,
			Microsoft::WRL::ComPtr<ID3D12PipelineState>,
			ComputePipelineKeyHash
		> mCompute;

		std::unordered_set<GraphicsPsoKey, GraphicsPipelineKeyHash>
		mDirtyGraphics;
		std::unordered_set<ComputePipelineKey, ComputePipelineKeyHash>
		mDirtyCompute;
	};
}
