#pragma once
#include <vector>

#include <engine/uphysics/PhysicsTypes.h>

namespace UPhysics {
	// フラットノード
	struct FlatNode {
		Unnamed::AABB     bounds;
		uint32_t leftFirst;
		uint32_t rightFirst;
		uint16_t primCount;
	};

	class BVHBuilder {
	public:
		void Build(
			const std::vector<Unnamed::Triangle>& triangles,
			std::vector<FlatNode>&       outNodes,
			std::vector<uint32_t>&       outTriIndices,
			uint32_t                     leafSize = 4
		);

	private:
		uint32_t Recurse(uint32_t start, uint32_t end, int depth);
		void SAHSplit(uint32_t start, uint32_t end, int axis, uint32_t& outMid);

		std::vector<TriInfo>  mTriInfos;
		std::vector<uint32_t> mTriIndices;
		std::vector<FlatNode> mNodes;
		uint32_t              mLeafSize = 4;
	};
}
