#include "UPrimitives.h"

namespace Unnamed {
	/// @brief 点でAABBを拡張します
	/// @param point 拡張する点
	void AABB::Expand(const Vec3& point) {
		min = Vec3::Min(min, point);
		max = Vec3::Max(max, point);
	}

	/// @brief AABBでAABBを拡張します
	/// @param aabb 拡張するAABB
	void AABB::Expand(const AABB& aabb) {
		Expand(aabb.min);
		Expand(aabb.max);
	}

	/// @brief AABBの中心を取得します
	/// @return AABBの中心座標
	Vec3 AABB::Center() const {
		return (min + max) * 0.5f;
	}

	/// @brief AABBの表面積を取得します
	/// @return AABBの表面積
	float AABB::SurfaceArea() const {
		const Vec3 d = max - min;
		return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
	}

	/// @brief AABBの最も長い軸を取得します
	/// @return 最も長い軸のインデックス(0=x, 1=y, 2=z)
	int AABB::LongestAxis() const {
		const Vec3 d = max - min;
		if (d.x >= d.y && d.x >= d.z) {
			return 0;
		}
		if (d.y >= d.x && d.y >= d.z) {
			return 1;
		}
		return 2;
	}

	/// @brief AABBのサイズを取得します
	/// @return AABBのサイズベクトル
	Vec3 AABB::Size() const {
		return max - min;
	}
}
