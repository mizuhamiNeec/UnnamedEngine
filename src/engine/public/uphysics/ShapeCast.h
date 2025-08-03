#pragma once
#include <engine/public/uphysics/Primitives.h>

namespace UPhysics {
	struct ShapeCast {
		virtual                    ~ShapeCast() = default;
		[[nodiscard]] virtual AABB ExpandNode(const AABB& nodeBounds) const = 0;

		bool TestTriangle(
			const Triangle& tri,
			const Vec3&     dir,
			float           length,
			float&          toi,
			Vec3&           normal
		);
	};
}
