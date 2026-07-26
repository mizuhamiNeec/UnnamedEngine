#pragma once

#include <engine/unnamed/primitive/Primitives.h>

namespace Unnamed::Physics {
	// ヒット情報
	/// @brief 形状照会の衝突位置、法線、距離、および対象を呼び出し元へ返します
	struct Hit {
		float    toi           = FLT_MAX; // 0～1（キャストのTOI）。Overlap系は1.0f
		float    depth         = 0.0f;
		Vec3     pos           = Vec3::zero;
		Vec3     normal        = Vec3::zero;
		uint32_t triIndex      = UINT_FAST32_MAX;
		uint64_t hitEntityGuid = 0;
		bool     startSolid    = false; // 開始時に形状が重なっていたか
		bool     allsolid      = false; // トレース全域で固体内だったか
	};

	// 形状情報
	/// @brief 衝突判定に使う三角形の頂点と面情報を保持します
	struct TriInfo {
		AABB     bounds;   // 境界
		Vec3     center;   // 中心
		uint32_t triIndex; // 三角形のインデックス
	};
}
