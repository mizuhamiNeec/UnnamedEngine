#include "SphereCast.h"

#include "CollisionDetection.h"

#include <cmath>

namespace Unnamed::Physics {
	/// @brief ノードのAABBを拡張します
	/// @param nodeBounds ノードのAABB
	/// @return 拡張されたAABB
	AABB SphereCast::ExpandNode(
		const AABB& nodeBounds
	) const {
		const auto r = Vec3(radius);
		// わずかなマージンを追加して数値誤差を防ぐ
		constexpr auto margin = Vec3(1e-6f);
		return {
			.min = nodeBounds.min - r - margin,
			.max =nodeBounds.max + r + margin
		};
	}

	/// @brief 三角形との衝突テストを行います
	/// @param triangle テストする三角形
	/// @param dir キャスト方向（正規化済み）
	/// @param length キャスト距離
	/// @param outTOI 衝突時刻の出力先
	/// @param outNormal 衝突法線の出力先
	/// @return 衝突したらtrue
	bool SphereCast::TestTriangle(
		const Triangle& triangle,
		const Vec3&     dir,
		const float     length,
		float&          outTOI,
		Vec3&           outNormal
	) const {
		return SweptSphereVsTriSAT(
			center,
			radius,
			dir * length,
			triangle,
			outTOI,
			outNormal
		);
	}

	/// @brief キャスト開始時点での重なりをチェックします
	/// @param triangle テストする三角形
	/// @param depth 重なりの深さの出力先
	/// @param normal 重なりの法線の出力先
	/// @return 重なっていたらtrue
	bool SphereCast::OverlapAtStart(
		const Triangle& triangle,
		float&          depth,
		Vec3&           normal
	) const {
		const Vec3  q    = ClosestPointOnTriangle(triangle, center);
		const Vec3  v    = center - q;
		const float dist = v.Length();
		if (dist < radius) {
			depth  = radius - dist;
			normal = dist > 1e-8f ?
				         v / dist :
				         (triangle.v1 - triangle.v0).Cross(
					         triangle.v2 - triangle.v0
				         ).Normalized();
			return true;
		}
		return false;
	}

	/// @brief 衝突点を計算します
	/// @param start キャスト開始位置
	/// @param dirNormalized キャスト方向（正規化済み）
	/// @param length キャスト距離
	/// @param toi 衝突時刻
	/// @param normal 衝突法線
	/// @return 衝突点の座標
	Vec3 SphereCast::ComputeImpactPoint(
		const Vec3& start,
		const Vec3& dirNormalized,
		const float length,
		const float toi,
		const Vec3& normal
	) const {
		const float travel       = toi * length;
		const Vec3  impactCenter = start + dirNormalized * travel;
		Vec3        n            = normal;
		const float nLenSq       = n.SqrLength();
		if (nLenSq > 1e-12f) {
			n /= std::sqrt(nLenSq);
		} else {
			return impactCenter;
		}
		return impactCenter - n * radius;
	}
}
