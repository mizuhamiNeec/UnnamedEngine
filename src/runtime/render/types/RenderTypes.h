#pragma once

#include <cstdint>
#include <dxgiformat.h>

#include <runtime/core/math/Math.h>

namespace Unnamed {
	struct BufferHandle {
		uint32_t id = 0;
	};

	struct MeshHandle {
		uint32_t           id  = UINT32_MAX;
		uint32_t           gen = 0;
		[[nodiscard]] bool IsValid() const { return id != UINT32_MAX; }
	};

	/// @brief 描画に使うカメラの情報
	struct RenderView {
		Mat4 view;
		Mat4 proj;
		Mat4 viewProj;
		Vec3 cameraPos;
	};

	struct GpuVB {
		BufferHandle handle;
		uint32_t     stride = 0;
	};

	struct GpuIB {
		BufferHandle handle;
		DXGI_FORMAT  format = DXGI_FORMAT_R32_UINT;
	};

	struct MeshGPU {
		GpuVB    vb;
		GpuIB    ib;
		uint32_t indexCount = 0;
		uint32_t firstIndex = 0;
		int32_t  baseVertex = 0;
	};

}