#pragma once

#include <runtime/core/math/Math.h>

namespace Unnamed {
	struct VertexPNUV {
		Vec3 position;
		Vec3 normal;
		Vec2 uv;
	};

	static_assert(sizeof(VertexPNUV) == 32, "Vertex layout mismatch");
}
