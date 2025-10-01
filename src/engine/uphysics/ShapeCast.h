#pragma once
#include <engine/uphysics/PhysicsTypes.h>

namespace UPhysics {
	struct ShapeCast {
		virtual                             ~ShapeCast() = default;
		[[nodiscard]] virtual Unnamed::AABB ExpandNode(const Unnamed::AABB& nodeBounds) const = 0;

		virtual bool TestTriangle(
			const Unnamed::Triangle& tri,
			const Vec3&     dir,
			float           length,
			float&          toi,
			Vec3&           normal
		) const = 0;
	};
}
