#pragma once

#include <d3d12.h>
#include <d3dx12.h>

#include <engine/urootsignaturecache/RootSignatureCache.h>

#include "runtime/render/resources/ShaderLibrary.h"

namespace Unnamed {
	struct InputElement {
		const char* semantic;
		UINT        index;
		DXGI_FORMAT format;
		UINT        offset;
	};

	struct PipelineDesc {
		RootSignatureHandle       rootSignature;
		std::vector<InputElement> inputLayout;
		DXGI_FORMAT               rtv[8] = {DXGI_FORMAT_R8G8B8A8_UNORM};
		UINT                      numRt  = 1;
		DXGI_FORMAT               dsv    = DXGI_FORMAT_UNKNOWN;

		D3D12_BLEND_DESC      blend      = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		D3D12_RASTERIZER_DESC rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		D3D12_DEPTH_STENCIL_DESC depth = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

		D3D12_PRIMITIVE_TOPOLOGY_TYPE topology =
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		DXGI_SAMPLE_DESC sample = {1, 0};

		// シェーダー
		const ShaderBlob* vs = nullptr;
		const ShaderBlob* ps = nullptr;
		//const ShaderBlob* gs = nullptr;
	};

	struct PsoHandle {
		uint32_t id = UINT32_MAX;
	};

	class UPipelineCache {
	public:
		explicit UPipelineCache(
			GraphicsDevice*     graphicsDevice,
			RootSignatureCache* rootSignatureCache
		);
		PsoHandle            GetOrCreate(const PipelineDesc& desc);
		ID3D12PipelineState* Get(PsoHandle handle) const;

	private:
		static size_t Hash(const PipelineDesc& desc);

	private:
		GraphicsDevice*     mGraphicsDevice;
		RootSignatureCache* mRootSignatureCache;

		struct Entry {
			size_t                                      hash;
			Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		};

		std::vector<Entry>                   mEntries;
		std::unordered_map<size_t, uint32_t> mHashToIndex;
	};
}
