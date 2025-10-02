#include <engine/uphysics/BVHBuilder.h>

namespace UPhysics {
	void BVHBuilder::Build(
		const std::vector<Unnamed::Triangle>& triangles,
		std::vector<FlatNode>&       outNodes,
		std::vector<uint32_t>&       outTriIndices,
		const uint32_t               leafSize
	) {
		mLeafSize = leafSize;
		size_t n  = triangles.size();
		mTriInfos.resize(n);
		mTriIndices.resize(n);

		for (size_t i = 0; i < n; ++i) {
			const Unnamed::Triangle& t    = triangles[i];
			TriInfo&        info = mTriInfos[i];
			info.bounds.Expand(t.v0);
			info.bounds.Expand(t.v1);
			info.bounds.Expand(t.v2);
			info.center    = (t.v0 + t.v1 + t.v2) / 3.0f;
			info.triIndex  = static_cast<uint32_t>(i);
			mTriIndices[i] = static_cast<uint32_t>(i);
		}

		// 再帰構築
		mNodes.reserve(n * 2);
		Recurse(0, static_cast<uint32_t>(n), 0);

		outNodes      = std::move(mNodes);
		outTriIndices = std::move(mTriIndices);
	}

	uint32_t BVHBuilder::Recurse(
		uint32_t start, uint32_t end,
		int      depth
	) {
		FlatNode node      = {};
		uint32_t nodeIndex = static_cast<uint32_t>(mNodes.size());
		mNodes.emplace_back(node);

		Unnamed::AABB bounds = {};
		for (uint32_t i = start; i < end; ++i) {
			bounds.Expand(mTriInfos[mTriIndices[i]].bounds);
		}
		mNodes[nodeIndex].bounds = bounds;

		uint32_t triCount = end - start;
		if (triCount <= mLeafSize) {
			mNodes[nodeIndex].leftFirst = start;
			mNodes[nodeIndex].primCount = static_cast<uint16_t>(triCount);
			return nodeIndex;
		}

		// ノードの分割
		int      axis = bounds.LongestAxis();
		uint32_t mid;
		SAHSplit(start, end, axis, mid);

		// 分割失敗
		if (mid == start || mid == end) {
			mid = (start + end) / 2;
		}

		// 子を深度優先でプッシュ
		const uint32_t left  = Recurse(start, mid, depth + 1);
		const uint32_t right = Recurse(mid, end, depth + 1);

		mNodes[nodeIndex].leftFirst  = left;
		mNodes[nodeIndex].rightFirst = right;
		mNodes[nodeIndex].primCount  = 0;
		return nodeIndex;
	}

	void BVHBuilder::SAHSplit(
		uint32_t start, uint32_t end,
		int      axis, uint32_t& outMid
	) {
		const int kBucket = 12;
		struct Bucket {
			Unnamed::AABB     bounds;
			uint32_t count = 0;
		};
		Bucket buckets[kBucket] = {};

		Unnamed::AABB centerBounds = {};
		for (uint32_t i = start; i < end; ++i) {
			centerBounds.Expand(mTriInfos[mTriIndices[i]].center);
		}

		float minC  = (&centerBounds.min.x)[axis];
		float maxC  = (&centerBounds.max.x)[axis];
		float scale = (maxC - minC > 1e-5f) ? (kBucket / (maxC - minC)) : 0.0f;

		// バケットに三角形を振り分け
		for (uint32_t i = start; i < end; ++i) {
			float c = (&mTriInfos[mTriIndices[i]].center.x)[axis];
			int b = std::min(kBucket - 1, static_cast<int>((c - minC) * scale));
			buckets[b].bounds.Expand(mTriInfos[mTriIndices[i]].bounds);
			buckets[b].count++;
		}

		// 前方後方累積でコストを計算a
		float    cost[kBucket - 1];
		Unnamed::AABB     left[kBucket - 1];
		Unnamed::AABB     right[kBucket - 1];
		uint32_t leftCount[kBucket - 1];
		uint32_t rightCount[kBucket - 1];

		Unnamed::AABB     t;
		uint32_t c = 0;
		for (int i = 0; i < kBucket - 1; ++i) {
			t.Expand(buckets[i].bounds);
			c += buckets[i].count;
			left[i]      = t;
			leftCount[i] = c;
		}
		t = {};
		c = 0;
		for (int i = kBucket - 1; i > 0; --i) {
			t.Expand(buckets[i].bounds);
			c += buckets[i].count;
			right[i - 1]      = t;
			rightCount[i - 1] = c;
		}
		for (int i = 0; i < kBucket - 1; ++i) {
			cost[i] = 0.125f +
				(leftCount[i]
					 ? left[i].SurfaceArea() * static_cast<float>(leftCount[i])
					 : 0.0f) +
				(rightCount[i]
					 ? right[i].SurfaceArea() * static_cast<float>(rightCount[
						 i])
					 : 0.0f);
		}

		// 最小コストのバケットをMidにする
		int   best     = int(std::min_element(cost, cost + kBucket - 1) - cost);
		float splitPos = minC + static_cast<float>(best + 1) / scale;

		size_t mid_raw = std::partition(
			mTriIndices.begin() + start,
			mTriIndices.begin() + end,
			[&](const uint32_t index) {
				return (&mTriInfos[index].center.x)[axis] < splitPos;
			}
		) - mTriIndices.begin();
		outMid = static_cast<uint32_t>(mid_raw); // 明示的キャストで警告回避
	}
}
