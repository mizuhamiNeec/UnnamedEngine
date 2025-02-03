#include "PhysicsEngine.h"

#include <type_traits>

#include <Components/ColliderComponent/MeshColliderComponent.h>
#include <Components/ColliderComponent/Base/ColliderComponent.h>

#include <Debug/Debug.h>

#include <Entity/Base/Entity.h>

#include <SubSystem/Console/Console.h>

void PhysicsEngine::Init() {
	dynamicBVH_.Clear();
	staticBVH_.Clear();
	registeredEntities_.clear();
	colliderComponents_.clear();
	staticMeshes_.clear();
	dynamicMeshes_.clear();
}

void PhysicsEngine::Update([[maybe_unused]] float deltaTime) {
	UpdateBVH();

	// TODO: 物理シミュレーション(やる気が出たら)
}

void PhysicsEngine::RegisterEntity(Entity* entity, bool isStatic) { // 既に登録されているか、エンティティがnullptrの場合は登録しない
	if (!entity || registeredEntities_.contains(entity)) {
		Console::Print(
			"エンティティが登録済みかnullptrです\n",
			kConTextColorWarning,
			Channel::Physics);
		return;
	}

	auto* collider = entity->GetComponent<ColliderComponent>();
	if (!collider) {
		Console::Print(
			std::format(
				"Entity: '{}' にはColliderComponentがアタッチされていません\n",
				entity->GetName()),
			kConTextColorWarning,
			Channel::Physics);
		return;
	}

	if (auto* meshCollider = dynamic_cast<MeshColliderComponent*>(collider)) {
		auto* transform = entity->GetTransform();
		const std::vector<Triangle>& triangles = meshCollider->GetTriangles();

		if (isStatic) {
			// 静的メッシュはワールド空間に変換して登録
			StaticMeshData meshData;
			meshData.worldTriangles.reserve(triangles.size());
			Mat4 worldMatrix = transform->GetWorldMat();

			// ローカルBVHの構築
			meshData.localBVH.Clear();
			for (size_t i = 0; i < triangles.size(); ++i) {
				// 三角形をワールド空間に変換
				Triangle worldTri = { Vec3::zero, Vec3::zero, Vec3::zero };
				worldTri.v0 = Mat4::Transform(triangles[i].v0, worldMatrix);
				worldTri.v1 = Mat4::Transform(triangles[i].v1, worldMatrix);
				worldTri.v2 = Mat4::Transform(triangles[i].v2, worldMatrix);
				meshData.worldTriangles.push_back(worldTri);

				// 三角形のAABBを計算してローカルBVHに登録
				AABB triAABB = FromTriangle(worldTri);
				meshData.localBVH.InsertObject(triAABB, static_cast<int>(i));
			}

			// メッシュ全体のAABBを計算
			meshData.worldBounds = FromTriangles(meshData.worldTriangles);
			staticMeshes_.emplace(collider, std::move(meshData));
		} else {
			// 動的メッシュはローカルのまま登録
			DynamicMeshData meshData;
			meshData.localTriangles = triangles;
			meshData.transform = transform;
			dynamicMeshes_.emplace(collider, std::move(meshData));
		}
	}

	collider->SetPhysicsEngine(this);

	// コライダーコンポーネントとエンティティを登録
	colliderComponents_.push_back(collider);
	registeredEntities_.insert(entity);

	// コライダーのAABBを取得
	AABB aabb = collider->GetBoundingBox();

	// インデックスを取得（新たに追加されたコライダーの位置）
	int colliderIndex = static_cast<int>(colliderComponents_.size()) - 1;

	// BVHにAABBとコライダーのポインタを登録
	int nodeId = -1;
	if (isStatic) {
		nodeId = staticBVH_.InsertObject(aabb, colliderIndex);
	} else {
		nodeId = dynamicBVH_.InsertObject(aabb, colliderIndex);
	}

	// コライダーとnodeIdのマップを保存
	colliderNodeIds_[collider] = nodeId;
}

void PhysicsEngine::UnregisterEntity(Entity* entity) {
	if (!entity || !registeredEntities_.contains(entity)) {
		Console::Print(
			"エンティティが登録されていないかnullptrです\n",
			kConTextColorWarning,
			Channel::Physics);
		return;
	}

	auto* collider = entity->GetComponent<ColliderComponent>();
	if (!collider) {
		Console::Print(
			std::format(
				"Entity: '{}' にはColliderComponentがアタッチされていません\n",
				entity->GetName()),
			kConTextColorWarning,
			Channel::Physics);
		return;
	}

	if (auto* meshCollider = dynamic_cast<MeshColliderComponent*>(collider)) {
		// 静的/動的メッシュデータの削除
		staticMeshes_.erase(meshCollider);
		dynamicMeshes_.erase(meshCollider);
	}

	// コライダーからNodeIDを取得
	auto it = colliderNodeIds_.find(collider);
	if (it != colliderNodeIds_.end()) {
		int nodeId = it->second;

		// BVHからオブジェクトを削除
		staticBVH_.RemoveObject(nodeId);
		dynamicBVH_.RemoveObject(nodeId);

		colliderNodeIds_.erase(it);
	}

	// 登録リストから削除
	std::erase(colliderComponents_, collider);
	registeredEntities_.erase(entity);
}

std::vector<HitResult> PhysicsEngine::BoxCast(
	const Vec3& start, const Vec3& direction, float distance, const Vec3& halfSize) {
	std::vector<HitResult> hitResults;

	// 正規化
	Vec3 dir = direction.Normalized();

	// 移動量
	Vec3 moveAmount = dir * distance;

	// 開始AABBと終了AABBを計算
	AABB startBox(start - halfSize, start + halfSize);
	Vec3 endPos = start + moveAmount;
	AABB endBox(endPos - halfSize, endPos + halfSize);

	// スイープ軌跡のAABB（開始AABBと終了AABBの包囲）
	AABB sweepBox = {
		sweepBox.min = Vec3::Min(startBox.min, endBox.min),
		sweepBox.max = Vec3::Max(startBox.max, endBox.max)
	};

	/*Debug::DrawBox(startBox.GetCenter(), Quaternion::identity, startBox.GetSize(), Vec4::brown);
	Debug::DrawBox(endBox.GetCenter(), Quaternion::identity, endBox.GetSize(), Vec4::orange);*/

	// BVHから重なり候補のインデックスを取得（static/dynamicともに）
	std::vector<int> candidateIndices;
	auto addCandidates = [&](const std::vector<int>& indices) {
		for (int idx : indices) {
			candidateIndices.push_back(idx);
		}
		};
	addCandidates(staticBVH_.QueryOverlaps(sweepBox));
	addCandidates(dynamicBVH_.QueryOverlaps(sweepBox));

	// 重複インデックスを削除
	std::unordered_set<int> uniqueCandidates(candidateIndices.begin(), candidateIndices.end());
	candidateIndices.assign(uniqueCandidates.begin(), uniqueCandidates.end());

	// 移動軌跡上を離散サンプリングして衝突判定を行う
	const int steps = 10;
	float dt = distance / steps;
	for (int i = 0; i <= steps; i++) {
		float t = i * dt;
		Vec3 curPos = start + dir * t;
		AABB currentBox(curPos - halfSize, curPos + halfSize);

		// 各候補オブジェクトに対して
		for (int idx : candidateIndices) {
			ColliderComponent* collider = colliderComponents_[idx];
			Entity* entity = collider->GetOwner();

			MeshColliderComponent* meshCollider = dynamic_cast<MeshColliderComponent*>(collider);
			if (meshCollider) {
				// MeshColliderの場合、各三角形との衝突判定
				const auto& triangles = meshCollider->GetTriangles(); // 三角形リスト取得（実装依存）
				for (const auto& tri : triangles) {
					HitResult hr;
					if (IntersectAABBWithTriangle(currentBox, tri, hr)) {
						// 衝突があった場合、tの値をヒット距離として設定
						hr.dist = t;
						// 現在位置を衝突点として採用（より詳細な衝突点計算は各自で実装）
						hr.hitPos = curPos;
						hr.hitEntity = entity;
						hitResults.push_back(hr);
					}
				}
			}
			// ※他のコライダータイプがある場合は、ここに処理を追加
		}
	}

	// 衝突結果を開始点からの距離順にソート
	std::ranges::sort(hitResults, [](const HitResult& a, const HitResult& b) { return a.dist < b.dist; });
	return hitResults;
}

std::vector<HitResult> PhysicsEngine::RayCast(const Vec3& start, const Vec3& direction, float distance) {
	std::vector<HitResult> hitResults;
	// 正規化された方向と終了点
	Vec3 dir = direction.Normalized();
	Vec3 endPos = start + dir * distance;

	// レイのAABBを作成(開始点と終了点の各軸のmin/max)
	Vec3 aabbMin = Vec3::Min(start, endPos);
	Vec3 aabbMax = Vec3::Max(start, endPos);
	AABB sweepBox(aabbMin, aabbMax);

	// BVHからオーバーラップ候補を取得
	auto dynamicOverlaps = dynamicBVH_.QueryOverlaps(sweepBox);
	auto staticOverlaps = staticBVH_.QueryOverlaps(sweepBox);

	// 各MeshColliderに対してレイキャストを処理
	auto processRaycastOnMesh = [&](ColliderComponent* collider, bool isStatic) {
		// 静的メッシュ
		if (isStatic) {
			auto it = staticMeshes_.find(collider);
			if (it != staticMeshes_.end()) {
				const auto& triangles = it->second.worldTriangles;
				for (const auto& tri : triangles) {
					float time;
					if (Physics::RayIntersectsTriangle(start, dir, tri, time) && time <= distance) {
						HitResult hit;
						hit.isHit = true;
						hit.dist = time;
						hit.hitPos = start + dir * time;
						hit.hitNormal = tri.GetNormal();
						hit.hitEntity = collider->GetOwner();
						hitResults.push_back(hit);
					}
				}
			}
		}
		// 動的メッシュ
		else {
			// auto it = dynamicMeshes_.find(collider);
			// if (it != dynamicMeshes_.end()) {
			//	const auto& meshData = it->second;
			//	// transform->TransformPoint() が存在する前提でローカル座標をワールドに変換
			//	for (const auto& localTri : meshData.localTriangles) {
			//		Triangle worldTri(
			//			meshData.transform->TransformPoint(tri.v0),
			//			meshData.transform->TransformPoint(tri.v1),
			//			meshData.transform->TransformPoint(tri.v2));
			//		float time;
			//		if (RayIntersectsTriangle(start, dir, worldTri, time) && time <= distance) {
			//			HitResult hit;
			//			hit.isHit = true;
			//			hit.dist = time;
			//			hit.hitPos = start + dir * time;
			//			hit.hitNormal = worldTri.GetNormal();
			//			hit.hitEntity = collider->GetOwner();
			//			hitResults.push_back(hit);
			//		}
			//	}
			// }
		}
		};

	// 動的オブジェクトの処理
	for (int i : dynamicOverlaps) {
		ColliderComponent* collider = colliderComponents_[i];
		processRaycastOnMesh(collider, false);
	}
	// 静的オブジェクトの処理
	for (int i : staticOverlaps) {
		ColliderComponent* collider = colliderComponents_[i];
		processRaycastOnMesh(collider, true);
	}

	// 衝突結果を距離順にソート
	std::ranges::sort(
		hitResults, [](const HitResult& a, const HitResult& b) { return a.dist < b.dist; });
	return hitResults;
}

bool PhysicsEngine::IntersectAABBWithTriangle(
	const AABB& aabb, const Triangle& triangle, HitResult& outHit) {
	// AABBの中心と半径を取得
	Vec3 aabbCenter = aabb.GetCenter();
	Vec3 aabbHalfSize = aabb.GetHalfSize();

	// 三角形の頂点をAABBのローカル座標系に変換
	Vec3 v0 = triangle.v0 - aabbCenter;
	Vec3 v1 = triangle.v1 - aabbCenter;
	Vec3 v2 = triangle.v2 - aabbCenter;

	// AABBの軸
	Vec3 axes[] = {
		Vec3::right, Vec3::up, Vec3::forward };

	// テスト1: AABBの各軸に対する投影
	for (int i = 0; i < 3; ++i) {
		float r = aabbHalfSize[i];
		float p0 = v0[i];
		float p1 = v1[i];
		float p2 = v2[i];
		float minP = (std::min)({ p0, p1, p2 });
		float maxP = (std::max)({ p0, p1, p2 });
		if (minP > r || maxP < -r) {
			return false; // 分離軸発見
		}
	}

	// 三角形の法線
	Vec3 normal = triangle.GetNormal();

	// テスト2: 三角形の法線に対する投影
	if (!normal.IsZero()) {
		bool overlap = OverlapOnAxis(aabbHalfSize, v0, v1, v2, normal);
		if (!overlap) {
			return false; // 分離軸発見
		}
	}

	// テスト3: 三角形の各エッジとAABBの各軸の外積に対する投影(9軸)
	Vec3 edges[3] = {
		v1 - v0,
		v2 - v1,
		v0 - v2 };

	for (const Vec3& edge : edges) {
		for (const Vec3& a : axes) {
			Vec3 testAxis = edge.Cross(a);
			if (!testAxis.IsZero()) {
				bool overlap = OverlapOnAxis(aabbHalfSize, v0, v1, v2, testAxis);
				if (!overlap) {
					return false; // 分離軸発見
				}
			}
		}
	}

	// よくここまで来たな...喜べ衝突だ!
	outHit.isHit = true;
	outHit.hitNormal = normal;

	// 衝突点の計算
	Vec3 closestPoint = ClosestPointOnTriangleToPoint(Vec3::zero, triangle);
	outHit.hitPos = aabbCenter + closestPoint; // AABBのローカル座標からワールド座標へ

	// 深さの計算 (AABB表面との最近距離)
	Vec3 penetration = ClosestPointOnAABBToPoint(closestPoint, aabb) - closestPoint;
	outHit.depth = penetration.Length();

	// その他の情報を設定
	outHit.dist = outHit.depth; // 距離 = 深さとして扱う
	outHit.hitEntity = nullptr; // 必要に応じて設定

	return true;
}

Vec3 PhysicsEngine::ClosestPointOnTriangleToPoint(const Vec3& point, const Triangle& triangle) {
	// 頂点を取得
	const Vec3& v0 = triangle.v0;
	const Vec3& v1 = triangle.v1;
	const Vec3& v2 = triangle.v2;

	// 三角形の辺
	Vec3 edge0 = v1 - v0;
	Vec3 edge1 = v2 - v0;

	// 点から三角形の頂点へのベクトル
	Vec3 v0ToPoint = point - v0;

	// 各ベクトルのドット積を計算
	float a = edge0.Dot(edge0);
	float b = edge0.Dot(edge1);
	float c = edge1.Dot(edge1);
	float d = edge0.Dot(v0ToPoint);
	float e = edge1.Dot(v0ToPoint);

	// パラメータを計算
	float det = a * c - b * b;
	float s = b * e - c * d;
	float t = b * d - a * e;

	// 三角形の外部にある場合の修正
	if (s + t <= det) {
		if (s < 0.0f) {
			if (t < 0.0f) {
				// 頂点v0が最も近い
				s = 0.0f;
				t = 0.0f;
			} else {
				// 辺v0-v2上に投影
				s = 0.0f;
				t = e / c;
			}
		} else if (t < 0.0f) {
			// 辺v0-v1上に投影
			s = d / a;
			t = 0.0f;
		}
	} else {
		if (s < 0.0f) {
			// 辺v1-v2上に投影
			s = 0.0f;
			t = e / c;
		} else if (t < 0.0f) {
			// 辺v0-v1上に投影
			s = d / a;
			t = 0.0f;
		}
	}

	s = std::clamp(s / det, 0.0f, 1.0f);
	t = std::clamp(t / det, 0.0f, 1.0f);

	// 最近接点を計算
	return v0 + edge0 * s + edge1 * t;
}

Vec3 PhysicsEngine::ClosestPointOnAABBToPoint(const Vec3& point, const AABB& aabb) {
	Vec3 closestPoint = point;

	Vec3 min = aabb.GetCenter() - aabb.GetHalfSize();
	Vec3 max = aabb.GetCenter() + aabb.GetHalfSize();

	// 各軸でクランプ
	closestPoint.x = std::clamp(closestPoint.x, min.x, max.x);
	closestPoint.y = std::clamp(closestPoint.y, min.y, max.y);
	closestPoint.z = std::clamp(closestPoint.z, min.z, max.z);

	return closestPoint;
}

// std::vector<HitResult> PhysicsEngine::BoxCast(
//	const Vec3& start, const Vec3& direction, float distance, const Vec3& halfSize
//) {
// }

void PhysicsEngine::UpdateBVH() {
	for (auto* collider : colliderComponents_) {
		// 動的コライダーの更新処理
		if (collider->IsDynamic()) {
			AABB aabb = collider->GetBoundingBox();
			auto it = colliderNodeIds_.find(collider);
			if (it != colliderNodeIds_.end()) {
				int nodeId = it->second;
				// BVHからオブジェクトを削除
				dynamicBVH_.RemoveObject(nodeId);
				// BVHにオブジェクトを再登録
				nodeId = dynamicBVH_.InsertObject(aabb, nodeId);
				colliderNodeIds_[collider] = nodeId;
			}
		}
	}

	//// 動的BVHの描画
	// dynamicBVH_.DrawBvh(Vec4::lightGray);
	// dynamicBVH_.DrawObjects(Vec4::cyan);

	//// 静的BVHの描画
	// staticBVH_.DrawBvh(Vec4::darkGray);
	// staticBVH_.DrawObjects(Vec4::blue);

	//// 各静的メッシュのlocalBVHを描画
	// for (const auto& val : staticMeshes_ | std::views::values) {
	//	//const auto& meshData = val;

	//	// localBVHのノードを描画
	//	//meshData.localBVH.DrawBvh(Vec4::purple);
	//	// meshData.localBVH.DrawObjects(Vec4::red);
	//}
}

PhysicsEngine::StaticMeshData::StaticMeshData() {}
