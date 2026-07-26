#pragma once
#include <cstdint>
#include <d3d12.h>
#include <vector>

#include <wrl/client.h>

#include "engine/unnamed/primitive/Primitives.h"

namespace Unnamed::Render {
	/// @brief MeshSubMeshRangeは、submeshのindex開始位置、index数、material slotを保持します
	struct MeshSubMeshRange {
		uint32_t indexStart    = 0;
		uint32_t indexCount    = 0;
		uint32_t materialIndex = 0;
	};

	/// @brief MeshBufferは、vertex・index GPU bufferとsubmesh範囲列を所有します
	struct MeshBuffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> vb;
		Microsoft::WRL::ComPtr<ID3D12Resource> ib;

		D3D12_VERTEX_BUFFER_VIEW vbv = {};
		D3D12_INDEX_BUFFER_VIEW  ibv = {};

		uint32_t                      indexCount = 0;
		std::vector<MeshSubMeshRange> submeshes  = {};

		AABB localAABB;
	};
}
