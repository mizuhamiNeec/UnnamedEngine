#pragma once

#include <runtime/core/math/Math.h>

namespace Unnamed {
	/// @brief 位置、法線、UVを持つ頂点フォーマット
	struct VertexPNUV {
		Vec3 position;
		Vec3 normal;
		Vec2 uv;
	};

	static_assert(sizeof(VertexPNUV) == 32, "Vertex layout mismatch");
}
