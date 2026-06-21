#include <engine/unnamed/physics/CollisionDetection.h>
#include <engine/unnamed/physics/RayCast.h>

namespace Unnamed::Physics {
	/// @brief ノードの境界ボックスを拡張します
	/// @param nodeBounds ノードの境界ボックス
	/// @return 拡張された境界ボックス
	AABB RayCast::ExpandNode(const AABB& nodeBounds) const {
		return nodeBounds;
	}

	/// @brief 三角形とのレイキャストテストを行います
	/// @param tri テストする三角形
	/// @param dir レイの方向ベクトル（正規化済み）
	/// @param length レイの長さ
	/// @param toi 衝突時刻（0.0〜1.0の範囲）
	/// @param normal 衝突法線ベクトル
	/// @return 衝突したらtrue
	bool RayCast::TestTriangle(
		const Triangle& tri,
		const Vec3&     dir,
		const float     length,
		float&          toi,
		Vec3&           normal
	) const {
		float     t   = length;
		const Ray ray = {
			start,
			dir,
			invDir,
			0.0f,
			length
		};
		if (!TriangleVsRay(tri, ray, t, normal)) {
			return false; // 交差しなかった
		}
		toi = t / length;
		return true;
	}

	/// @brief レイの開始位置での重なり判定を行います
	/// @param tri テストする三角形
	/// @param depth 重なりの深さ
	/// @param normal 重なりの法線ベクトル
	/// @return 重なっていたらtrue
	bool RayCast::OverlapAtStart(
		const Triangle& /*tri*/,
		float& /*depth*/,
		Vec3& /*normal*/

	
	) const {
		// レイは体積を持たないため、開始時に"重なっている"概念は扱わない
		return false;
	}
}
