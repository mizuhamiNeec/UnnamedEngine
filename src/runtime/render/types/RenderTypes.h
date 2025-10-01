#pragma once

#include <runtime/core/math/Math.h>
#include <runtime/render/resources/RenderResourceManager.h>

/// @brief 描画に使うカメラの情報
struct RenderView {
	Mat4 view;
	Mat4 proj;
	Mat4 viewProj;
	Vec3 cameraPos;
};

struct MeshGPU {
	// ID3D12Resource*          vb = nullptr;
	// ID3D12Resource*          ib = nullptr;
	// D3D12_VERTEX_BUFFER_VIEW vbv{};
	// D3D12_INDEX_BUFFER_VIEW  ibv{};

	Unnamed::RenderResourceManager::GpuVB vb;
	Unnamed::RenderResourceManager::GpuIB ib;
	uint32_t                              indexCount = 0;
	uint32_t                              firstIndex = 0;
	int32_t                               baseVertex = 0;
};
