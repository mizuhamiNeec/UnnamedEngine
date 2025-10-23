#include <engine/uphysics/BoxCast.h>
#include <engine/uphysics/CollisionDetection.h>

#include <cmath>

namespace UPhysics {
	/// @brief ノードのAABBを拡張します
	/// @param nodeBounds ノードのAABB
	/// @return 拡張されたAABB
	Unnamed::AABB BoxCast::ExpandNode(const Unnamed::AABB& nodeBounds) const {
		return Unnamed::AABB{
			nodeBounds.min - half,
			nodeBounds.max + half
		};
	}

	/// @brief 三角形との衝突テストを行います
	/// @param triangle テストする三角形
	/// @param dir キャスト方向（正規化済み）
	/// @param length キャスト距離
	/// @param outTOI 衝突時刻の出力先
	/// @param outNormal 衝突法線の出力先
	/// @return 衝突したらtrue
	bool BoxCast::TestTriangle(
		const Unnamed::Triangle& triangle,
		const Vec3&              dir,
		const float              length,
		float&                   outTOI,
		Vec3&                    outNormal
	) const {
		return SweptAabbVsTriSAT(
			box,
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
	bool BoxCast::OverlapAtStart(
		const Unnamed::Triangle& triangle,
		float&                   depth,
		Vec3&                    normal
	) const {
		return BoxVsTriangleOverlap(box, triangle, normal, depth);
	}

	/// @brief 衝突点を計算します
	/// @param start キャスト開始位置
	/// @param dirNormalized キャスト方向（正規化済み）
	/// @param length キャスト距離
	/// @param toi 衝突時刻
	/// @param normal 衝突法線
	/// @return 衝突点の座標
	Vec3 BoxCast::ComputeImpactPoint(
		const Vec3& start,
		const Vec3& dirNormalized,
		float       length,
		float       toi,
		const Vec3& normal
	) const {
		const float travel = toi * length;
		Vec3        center = start + dirNormalized * travel;
		Vec3        n      = normal;
		const float nLenSq = n.SqrLength();
		if (nLenSq > 1e-12f) {
			n /= std::sqrt(nLenSq);
		} else {
			return center;
		}
		const Vec3  absN{std::fabs(n.x), std::fabs(n.y), std::fabs(n.z)};
		const float support = absN.x * half.x + absN.y * half.y + absN.z * half.
			z;
		return center - n * support;
	}
}
