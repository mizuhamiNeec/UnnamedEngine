#pragma once
#include <engine/uphysics/PhysicsTypes.h>

namespace UPhysics {
	struct ShapeCast {
		virtual                             ~ShapeCast() = default;
		[[nodiscard]] virtual Unnamed::AABB ExpandNode(
			const Unnamed::AABB& nodeBounds) const = 0;

		virtual bool TestTriangle(
			const Unnamed::Triangle& tri,
			const Vec3&              dir,
			float                    length,
			float&                   toi,
			Vec3&                    normal
		) const = 0;

		// 開始時(t=0)オーバーラップ検出
		// 重なっていれば true。depth はめり込み量（不明なら 0）
		virtual bool OverlapAtStart(
			const Unnamed::Triangle& tri,
			float&                   depth,
			Vec3&                    normal
		) const = 0;

		virtual Vec3 ComputeImpactPoint(
			const Vec3& start,
			const Vec3& dirNormalized,
			float       length,
			float       toi,
			const Vec3& normal
		) const {
			(void)normal;
			const float travel = toi * length;
			return start + dirNormalized * travel;
		}
	};
}
