#pragma once
#include <vector>

#include <engine/unnamed/physics/PhysicsTypes.h>

#include "../primitive/Primitives.h"

namespace Unnamed::Physics {
	// フラットノード
	/// @brief FlatNodeは、物理照会の階層またはグラフにおける接続関係とノード固有値を保持します
	struct FlatNode {
		AABB     bounds;
		uint32_t leftFirst;
		uint32_t rightFirst;
		uint16_t primCount;
	};

	/// @brief BVHビルダークラス
	class BVHBuilder {
	public:
		void Build(
			const std::vector<Triangle>& triangles,
			std::vector<FlatNode>&       outNodes,
			std::vector<uint32_t>&       outTriIndices,
			const uint32_t&              leafSize = 4
		);

	private:
		uint32_t Recurse(
			const uint32_t& start, const uint32_t& end, const int& depth
		);
		void SAHSplit(
			const uint32_t& start, const uint32_t& end, const int& axis,
			uint32_t&       outMid
		);

		std::vector<TriInfo>  mTriInfos;
		std::vector<uint32_t> mTriIndices;
		std::vector<FlatNode> mNodes;
		uint32_t              mLeafSize = 4;
	};
}
