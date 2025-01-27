#define NOMINMAX

#include <stack>
#include <type_traits>
#include <utility>
#include <vector>

#include <Debug/Debug.h>

#include <SubSystem/Console/Console.h>
#include <Lib/Math/Quaternion/Quaternion.h>
#include <Lib/Math/Vector/Vec3.h>
#include <Lib/Math/Vector/Vec4.h>

#include <Physics/Physics.h>

//-----------------------------------------------------------------------------
// Purpose: AABBのコンストラクタ
//-----------------------------------------------------------------------------
AABB::AABB(const Vec3& min, const Vec3& max) :
	min(min),
	max(max) {
}

//-----------------------------------------------------------------------------
// Purpose: AABBの中心を取得します
//-----------------------------------------------------------------------------
Vec3 AABB::GetCenter() const {
	return (min + max) * 0.5f;
}

//-----------------------------------------------------------------------------
// Purpose: AABBのサイズを取得します
//-----------------------------------------------------------------------------
Vec3 AABB::GetSize() const {
	return max - min;
}

//-----------------------------------------------------------------------------
// Purpose: AABBの半径を取得します
//-----------------------------------------------------------------------------
Vec3 AABB::GetHalfSize() const {
	return GetSize() * 0.5f;
}

//-----------------------------------------------------------------------------
// Purpose: AABB同士が交差しているかを判定します
//-----------------------------------------------------------------------------
bool AABB::Intersects(const AABB& other) const {
	__m128 minA = _mm_set_ps(min.x, min.y, min.z, 0.0f);
	__m128 maxA = _mm_set_ps(max.x, max.y, max.z, 0.0f);
	__m128 minB = _mm_set_ps(other.min.x, other.min.y, other.min.z, 0.0f);
	__m128 maxB = _mm_set_ps(other.max.x, other.max.y, other.max.z, 0.0f);

	__m128 cmp1 = _mm_cmple_ps(minA, maxB);
	__m128 cmp2 = _mm_cmpge_ps(maxA, minB);

	return (_mm_movemask_ps(_mm_and_ps(cmp1, cmp2)) & 0x7) == 0x7;
}

//-----------------------------------------------------------------------------
// Purpose: AABB同士を結合します
//-----------------------------------------------------------------------------
AABB AABB::Combine(const AABB& aabb) const {
	return {
		Vec3(
			std::min(min.x, aabb.min.x),
			std::min(min.y, aabb.min.y),
			std::min(min.z, aabb.min.z)
		),
		Vec3(
			std::max(max.x, aabb.max.x),
			std::max(max.y, aabb.max.y),
			std::max(max.z, aabb.max.z)
		)
	};
}

//-----------------------------------------------------------------------------
// Purpose: 体積を取得します
//-----------------------------------------------------------------------------
float AABB::Volume() const {
	const Vec3 size = GetSize();
	return size.x * size.y * size.z;
}

bool OverlapOnAxis(Vec3 aabbHalfSize, Vec3 v0, Vec3 v1, Vec3 v2, Vec3 axis) {
	// 軸を正規化
	Vec3 normalizedAxis = axis.Normalized();

	// AABBを軸に投影
	float r = aabbHalfSize.x * std::abs(normalizedAxis.x) +
		aabbHalfSize.y * std::abs(normalizedAxis.y) +
		aabbHalfSize.z * std::abs(normalizedAxis.z);

	// 三角形を軸に投影
	float p0 = v0.Dot(normalizedAxis);
	float p1 = v1.Dot(normalizedAxis);
	float p2 = v2.Dot(normalizedAxis);

	float minP = std::min({ p0, p1, p2 });
	float maxP = std::max({ p0, p1, p2 });

	// 投影区間の重なりチェック
	return !(minP > r || maxP < -r);
}

AABB FromTriangles(const std::vector<Triangle>& vector) {
	Vec3 min = Vec3::max;
	Vec3 max = Vec3::min;
	for (const auto& tri : vector) {
		min = Vec3::Min(min, tri.v0);
		min = Vec3::Min(min, tri.v1);
		min = Vec3::Min(min, tri.v2);
		max = Vec3::Max(max, tri.v0);
		max = Vec3::Max(max, tri.v1);
		max = Vec3::Max(max, tri.v2);
	}
	return { min, max };
}

AABB FromTriangle(const Triangle& triangle) {
	Vec3 min = Vec3::max;
	Vec3 max = Vec3::min;
	min = Vec3::Min(min, triangle.v0);
	min = Vec3::Min(min, triangle.v1);
	min = Vec3::Min(min, triangle.v2);
	max = Vec3::Max(max, triangle.v0);
	max = Vec3::Max(max, triangle.v1);
	max = Vec3::Max(max, triangle.v2);
	return { min, max };
}

//-----------------------------------------------------------------------------
// Purpose: Triangleのコンストラクタ
//-----------------------------------------------------------------------------
Triangle::Triangle(
	const Vec3& v0, const Vec3& v1, const Vec3& v2
) : v0(v0),
v1(v1),
v2(v2) {
}

//-----------------------------------------------------------------------------
// Purpose: 三角形の法線ベクトルを取得します
//-----------------------------------------------------------------------------
Vec3 Triangle::GetNormal() const {
	Vec3 edge0 = v1 - v0;
	Vec3 edge1 = v2 - v0;
	return edge0.Cross(edge1).Normalized();
}

//-----------------------------------------------------------------------------
// Purpose: 三角形の面積を取得します
//-----------------------------------------------------------------------------
float Triangle::GetArea() const {
	Vec3 e0 = v1 - v0;
	Vec3 e1 = v2 - v0;
	return e0.Cross(e1).Length() * 0.5f;
}

//-----------------------------------------------------------------------------
// Purpose: 三角形の中心を取得します
//-----------------------------------------------------------------------------
Vec3 Triangle::GetCenter() const {
	return (v0 + v1 + v2) / 3.0f;
}

bool Triangle::IsPointInside(const Vec3& point) const {
	// 三角形の3辺と平面法線を利用して点が内部かを判定
	const Vec3 normal = (v1 - v0).Cross(v2 - v0).Normalized();

	const Vec3 edge0 = v1 - v0;
	const Vec3 edge1 = v2 - v1;
	const Vec3 edge2 = v0 - v2;

	const Vec3 c0 = point - v0;
	const Vec3 c1 = point - v1;
	const Vec3 c2 = point - v2;

	// 各辺の外積が法線と同じ方向なら内部
	return (edge0.Cross(c0).Dot(normal) >= 0.0f &&
		edge1.Cross(c1).Dot(normal) >= 0.0f &&
		edge2.Cross(c2).Dot(normal) >= 0.0f);
}

Vec3 Triangle::GetVertex(const int index) const {
	switch (index) {
	case 0: return v0;
	case 1: return v1;
	case 2: return v2;
	default: return Vec3::zero;
	}
}

void DynamicBVH::Clear() {
	nodes_.clear();
	rootNode_ = -1;
}

int DynamicBVH::InsertObject(const AABB& objectAABB, int objectIndex) {
	std::unique_lock lock(bvhMutex_);

	// 新しいノードのIDを取得
	int nodeID = static_cast<int>(nodes_.size());

	// 新しいノードを追加
	nodes_.emplace_back(objectAABB, -1, -1, -1, objectIndex, true);

	// ルートノードが存在しない場合
	if (rootNode_ == -1) {
		rootNode_ = nodeID;
		//OptimizeMemoryLayout();
		return nodeID;
	}

	// 安全チェックを追加
	int currentNode = rootNode_;
	while (currentNode >= 0 && currentNode < nodes_.size() && !nodes_[currentNode].isLeaf) {
		const float leftCost = CalculateGrowth(nodes_[currentNode].leftChild, objectAABB);
		const float rightCost = CalculateGrowth(nodes_[currentNode].rightChild, objectAABB);
		currentNode = (leftCost < rightCost) ? nodes_[currentNode].leftChild : nodes_[currentNode].rightChild;
	}

	// 範囲チェック
	if (currentNode >= 0 && currentNode < nodes_.size()) {
		CreateNewParent(currentNode, nodeID);
		//OptimizeMemoryLayout();
	} else {
		// エラーハンドリング
		Console::Print(
			"BVHノードの範囲外アクセスが検出されました\n",
			kConsoleColorError,
			Channel::Physics
		);
	}

	return nodeID;
}

void DynamicBVH::RemoveObject(const int nodeId) {
	BVHNode& node = nodes_[nodeId];

	// ルートノードの場合
	if (node.parent == -1) {
		nodes_.clear();
		rootNode_ = -1;
		return;
	}

	// 親ノードと兄弟ノードを取得
	int parentId = node.parent;
	BVHNode& parent = nodes_[parentId];
	int siblingId = (parent.leftChild == nodeId) ? parent.rightChild : parent.leftChild;
	BVHNode& sibling = nodes_[siblingId];

	// 親の親ノード（祖父ノード）を取得
	int grandParentId = parent.parent;

	if (grandParentId != -1) {
		// 親の親が存在する場合、祖父ノードの子を兄弟ノードに置き換える
		BVHNode& grandParent = nodes_[grandParentId];
		if (grandParent.leftChild == parentId) {
			grandParent.leftChild = siblingId;
		} else {
			grandParent.rightChild = siblingId;
		}
		sibling.parent = grandParentId;
	} else {
		// 親の親がいない場合、兄弟ノードをルートに設定
		rootNode_ = siblingId;
		sibling.parent = -1;
	}

	// 削除したノードと親ノードを無効化
	nodes_[nodeId] = nodes_.back();
	nodes_.pop_back();

	nodes_[parentId] = nodes_.back();
	nodes_.pop_back();
}

void DynamicBVH::UpdateObject(const int nodeId, const AABB& newAABB) {
	// ノードのAABBを更新
	nodes_[nodeId].boundingBox = newAABB;

	// 上位ノードのAABBを更新
	int currentNodeId = nodes_[nodeId].parent;

	while (currentNodeId != -1) {
		BVHNode& currentNode = nodes_[currentNodeId];
		const BVHNode& leftChild = nodes_[currentNode.leftChild];
		const BVHNode& rightChild = nodes_[currentNode.rightChild];

		AABB combinedAABB = leftChild.boundingBox.Combine(rightChild.boundingBox);

		// AABBが変化した場合、更新を続ける
		if (currentNode.boundingBox.min != combinedAABB.min || currentNode.boundingBox.max != combinedAABB.max) {
			currentNode.boundingBox = combinedAABB;
			currentNodeId = currentNode.parent;
		} else {
			// 変化がない場合、更新終了
			break;
		}
	}
}

void DynamicBVH::ReserveNodes(const size_t capacity) {
	nodes_.reserve(capacity);
}

std::vector<int> DynamicBVH::QueryOverlaps(const AABB& queryBox) const {
	std::shared_lock lock(bvhMutex_);

	std::vector<int> overlappingObjects;
	std::stack<int> stack;
	stack.push(rootNode_);

	while (!stack.empty()) {
		const int nodeId = stack.top();
		stack.pop();

		if (nodeId == -1) {
			continue;
		}

		const BVHNode& node = nodes_[nodeId];
		if (!node.boundingBox.Intersects(queryBox)) {
			continue;
		}

		if (node.isLeaf) {
			overlappingObjects.push_back(node.objectIndex);
		} else {
			stack.push(node.leftChild);
			stack.push(node.rightChild);
		}
	}

	return overlappingObjects;
}

//-----------------------------------------------------------------------------
// Purpose: AABBの体積増加量を計算します
//-----------------------------------------------------------------------------
float DynamicBVH::CalculateGrowth(const int nodeId, const AABB& newAABB) const {
	if (nodeId == -1) {
		return newAABB.Volume();
	}

	const BVHNode& node = nodes_[nodeId];

	AABB combinedAABB = node.boundingBox.Combine(newAABB);
	return combinedAABB.Volume() - node.boundingBox.Volume();
}

void DynamicBVH::CreateNewParent(const int existingNodeId, const int newNodeId) {
	// 事前に必要なデータを取得
	AABB existingBox = nodes_[existingNodeId].boundingBox;
	AABB newBox = nodes_[newNodeId].boundingBox;
	int oldParent = nodes_[existingNodeId].parent;

	// 新しい親ノードを作成
	int newParentId = static_cast<int>(nodes_.size());
	nodes_.emplace_back(
		existingBox.Combine(newBox),
		existingNodeId,
		newNodeId,
		oldParent,
		-1,  // objectIndex
		false // isLeaf
	);

	// 子ノードの親を更新
	nodes_[existingNodeId].parent = newParentId;
	nodes_[newNodeId].parent = newParentId;

	// ルートの更新
	if (existingNodeId == rootNode_) {
		rootNode_ = newParentId;
	} else {
		// 既存の親ノードの子を更新
		int parentIndex = oldParent;
		if (parentIndex >= 0 && parentIndex < nodes_.size()) {
			auto& parentNode = nodes_[parentIndex];
			if (parentNode.leftChild == existingNodeId) {
				parentNode.leftChild = newParentId;
			} else if (parentNode.rightChild == existingNodeId) {
				parentNode.rightChild = newParentId;
			}
		}
	}
}

void DynamicBVH::DrawBvhNode(const int nodeId, const Vec4& color) const {
	if (nodeId == -1) {
		return;
	}
	const BVHNode& node = nodes_[nodeId];
	Debug::DrawBox(
		node.boundingBox.GetCenter(), Quaternion::identity,
		node.boundingBox.GetHalfSize() * 2.0f, color
	);

	// 再帰をループに置き換え
	std::stack<int> stack;
	stack.push(nodeId);

	while (!stack.empty()) {
		int currentNodeId = stack.top();
		stack.pop();

		const BVHNode& currentNode = nodes_[currentNodeId];
		if (!currentNode.isLeaf) {
			const BVHNode& leftChild = nodes_[currentNode.leftChild];
			const BVHNode& rightChild = nodes_[currentNode.rightChild];
			Debug::DrawLine(
				currentNode.boundingBox.GetCenter(), leftChild.boundingBox.GetCenter(),
				Vec4::green
			);
			Debug::DrawLine(
				currentNode.boundingBox.GetCenter(), rightChild.boundingBox.GetCenter(),
				Vec4::red
			);
			Debug::DrawBox(
				leftChild.boundingBox.GetCenter(), Quaternion::identity,
				leftChild.boundingBox.GetHalfSize() * 2.0f, color
			);
			Debug::DrawBox(
				rightChild.boundingBox.GetCenter(), Quaternion::identity,
				rightChild.boundingBox.GetHalfSize() * 2.0f, color
			);
			stack.push(currentNode.leftChild);
			stack.push(currentNode.rightChild);
		}
	}
}

void DynamicBVH::DrawBvh(const Vec4& color) const {
	DrawBvhNode(rootNode_, color);
}

void DynamicBVH::DrawObjects(const Vec4& color) const {
	for (const BVHNode& node : nodes_) {
		if (node.isLeaf) {
			Debug::DrawBox(
				node.boundingBox.GetCenter(), Quaternion::identity,
				node.boundingBox.GetHalfSize() * 2.0f, color
			);
		}
	}
}

void DynamicBVH::OptimizeMemoryLayout() {
	// ノードを連続したメモリブロックに再配置
	std::vector<BVHNode> optimizedNodes;
	optimizedNodes.reserve(nodes_.size());

	std::stack<int> stack;
	stack.push(rootNode_);

	while (!stack.empty()) {
		int nodeId = stack.top();
		stack.pop();

		if (nodeId == -1) {
			continue;
		}

		optimizedNodes.push_back(nodes_[nodeId]);

		const BVHNode& node = nodes_[nodeId];
		if (!node.isLeaf) {
			stack.push(node.leftChild);
			stack.push(node.rightChild);
		}
	}

	nodes_ = std::move(optimizedNodes);
}
