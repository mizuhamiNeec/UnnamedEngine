#include <engine/public/Physics/PhysicsEngine.h>

#include <type_traits>

#include "engine/public/Components/ColliderComponent/MeshColliderComponent.h"
#include "engine/public/Debug/Debug.h"
#include "engine/public/Entity/Entity.h"
#include "engine/public/OldConsole/Console.h"

void PhysicsEngine::Init() {
	mDynamicBVH.Clear();
	mStaticBVH.Clear();
	mRegisteredEntities.clear();
	mColliderComponents.clear();
	mStaticMeshes.clear();
	mDynamicMeshes.clear();
}

void PhysicsEngine::Update([[maybe_unused]] float deltaTime) {
	for (auto collider : mColliderComponents) {
		if (!collider) {
			mColliderComponents.erase(
				std::ranges::remove(mColliderComponents,
				                    collider).begin(),
				mColliderComponents.end()
			);
		}
	}
	UpdateBVH();

	// TODO: 物理シミュレーション(やる気が出たら)
}

void PhysicsEngine::RegisterEntity(Entity* entity, bool isStatic) {
	// 既に登録されているか、エンティティがnullptrの場合は登録しない
	if (!entity || mRegisteredEntities.contains(entity)) {
		Console::Print(
			"エンティティが登録済みかnullptrです\n",
			kConTextColorWarning,
			Channel::Physics
		);
		return;
	}

	auto* collider = entity->GetComponent<ColliderComponent>();
	if (!collider) {
		Console::Print(
			std::format(
				"Entity: '{}' にはColliderComponentがアタッチされていません\n",
				entity->GetName()
			),
			kConTextColorWarning,
			Channel::Physics
		);
		return;
	}

	if (auto* meshCollider = dynamic_cast<MeshColliderComponent*>(collider)) {
		const auto*                  transform = entity->GetTransform();
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
				Triangle worldTri = {Vec3::zero, Vec3::zero, Vec3::zero};
				worldTri.v0 = Mat4::Transform(triangles[i].v0, worldMatrix);
				worldTri.v1 = Mat4::Transform(triangles[i].v1, worldMatrix);
				worldTri.v2 = Mat4::Transform(triangles[i].v2, worldMatrix);
				meshData.worldTriangles.emplace_back(worldTri);

				// 三角形のAABBを計算してローカルBVHに登録
				AABB triAABB = FromTriangle(worldTri);
				meshData.localBVH.InsertObject(triAABB, static_cast<int>(i));
			}

			// メッシュ全体のAABBを計算
			meshData.worldBounds = FromTriangles(meshData.worldTriangles);
			mStaticMeshes.emplace(collider, std::move(meshData));
		} else {
			// 動的メッシュはローカルのまま登録
			DynamicMeshData meshData;
			meshData.localTriangles = triangles;
			meshData.transform      = transform;
			mDynamicMeshes.emplace(collider, std::move(meshData));
		}
	}

	collider->SetPhysicsEngine(this);

	// コライダーコンポーネントとエンティティを登録
	mColliderComponents.emplace_back(collider);
	mRegisteredEntities.insert(entity);

	// コライダーのAABBを取得
	AABB aabb = collider->GetBoundingBox();

	// インデックスを取得（新たに追加されたコライダーの位置）
	int colliderIndex = static_cast<int>(mColliderComponents.size()) - 1;

	// BVHにAABBとコライダーのポインタを登録
	int nodeId = -1;
	if (isStatic) {
		nodeId = mStaticBVH.InsertObject(aabb, colliderIndex);
	} else {
		nodeId = mDynamicBVH.InsertObject(aabb, colliderIndex);
	}

	// コライダーとnodeIdのマップを保存
	mColliderNodeIds[collider] = nodeId;
}

void PhysicsEngine::UnregisterEntity(Entity* entity) {
	if (!entity) {
		Console::Print(
			"エンティティが登録されていないかnullptrです\n",
			kConTextColorWarning,
			Channel::Physics
		);
		return;
	}

	if (!mRegisteredEntities.contains(entity)) {
		Console::Print(
			"エンティティが登録されていないかnullptrです\n",
			kConTextColorWarning,
			Channel::Physics
		);
		return;
	}

	auto* collider = entity->GetComponent<ColliderComponent>();
	if (!collider) {
		Console::Print(
			std::format(
				"Entity: '{}' にはColliderComponentがアタッチされていません\n",
				entity->GetName()
			),
			kConTextColorWarning,
			Channel::Physics
		);
		return;
	}

	if (auto* meshCollider = dynamic_cast<MeshColliderComponent*>(collider)) {
		// 静的/動的メッシュデータの削除
		mStaticMeshes.erase(meshCollider);
		mDynamicMeshes.erase(meshCollider);
	}

	// コライダーからNodeIDを取得
	const auto it = mColliderNodeIds.find(collider);
	if (it != mColliderNodeIds.end()) {
		int nodeId = it->second;

		// BVHからオブジェクトを削除
		mStaticBVH.RemoveObject(nodeId);
		mDynamicBVH.RemoveObject(nodeId);

		mColliderNodeIds.erase(it);
	}

	// 登録リストから削除
	std::erase(mColliderComponents, collider);
	mRegisteredEntities.erase(entity);
}

std::vector<HitResult> PhysicsEngine::BoxCast(
	const Vec3& start, const Vec3& direction, float distance,
	const Vec3& halfSize
) const {
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
	AABB sweepBox    = {
		sweepBox.min = Math::Min(startBox.min, endBox.min),
		sweepBox.max = Math::Max(startBox.max, endBox.max)
	};

	Debug::DrawBox(startBox.GetCenter(), Quaternion::identity,
	               startBox.GetSize(), Vec4::brown);

	// BVHから重なり候補のインデックスを取得（static/dynamicともに）
	std::vector<int> candidateIndices;
	auto             addCandidates = [&](const std::vector<int>& indices) {
		for (int idx : indices) {
			candidateIndices.emplace_back(idx);
		}
	};
	addCandidates(mStaticBVH.QueryOverlaps(sweepBox));
	addCandidates(mDynamicBVH.QueryOverlaps(sweepBox));

	// 重複インデックスを削除
	std::unordered_set uniqueCandidates(candidateIndices.begin(),
	                                    candidateIndices.end());
	candidateIndices.assign(uniqueCandidates.begin(), uniqueCandidates.end());

	// 衝突判定を行う
	constexpr int steps = 4;
	float         dt    = distance / steps;
	for (int i = 0; i <= steps; i++) {
		float t      = i * dt;
		Vec3  curPos = start + dir * t;
		AABB  currentBox(curPos - halfSize, curPos + halfSize);

		// 各候補オブジェクトに対して
		for (int idx : candidateIndices) {
			ColliderComponent* collider = mColliderComponents[idx];
			Entity*            entity   = collider->GetOwner();

			auto meshCollider = dynamic_cast<MeshColliderComponent*>(collider);
			if (meshCollider) {
				const auto& triangles = meshCollider->GetTriangles();
				for (const auto& tri : triangles) {
					HitResult hr;
					if (IntersectAABBWithTriangle(currentBox, tri, hr)) {
						// 衝突があった場合、tの値をヒット距離として設定
						hr.dist = t;
						// TODO: より正確なヒット位置を計算する
						hr.hitPos    = curPos;
						hr.hitEntity = entity;

						Debug::DrawTriangle(tri, Vec4::cyan);
						hitResults.emplace_back(hr);
					}
				}
			}
		}
	}

	// 衝突結果を開始点からの距離順にソート
	std::ranges::sort(hitResults, [](const HitResult& a, const HitResult& b) {
		return a.dist < b.dist;
	});
	return hitResults;
}

std::vector<HitResult> PhysicsEngine::RayCast(const Vec3& start,
                                              const Vec3& direction,
                                              const float distance) {
	std::vector<HitResult> hitResults;
	// 正規化された方向と終了点
	const Vec3 dir    = direction.Normalized();
	const Vec3 endPos = start + dir * distance;

	// レイのAABBを作成(開始点と終了点の各軸のmin/max)
	const Vec3 aabbMin = Math::Min(start, endPos);
	const Vec3 aabbMax = Math::Max(start, endPos);
	const AABB sweepBox(aabbMin, aabbMax);

	// BVHからオーバーラップ候補を取得
	const auto dynamicOverlaps = mDynamicBVH.QueryOverlaps(sweepBox);
	const auto staticOverlaps  = mStaticBVH.QueryOverlaps(sweepBox);

	// 各MeshColliderに対してレイキャストを処理
	auto processRaycastOnMesh = [&](ColliderComponent* collider,
	                                const bool         isStatic) {
		if (!collider) {
			return;
		}

		// 静的メッシュ
		if (isStatic) {
			const auto it = mStaticMeshes.find(collider);
			if (it != mStaticMeshes.end()) {
				const auto& triangles = it->second.worldTriangles;
				for (const auto& tri : triangles) {
					float time;
					if (Physics::RayIntersectsTriangle(start, dir, tri, time) &&
						time <= distance) {
						HitResult hit;
						hit.isHit     = true;
						hit.dist      = time;
						hit.hitPos    = start + dir * time;
						hit.hitNormal = tri.GetNormal();
						hit.hitEntity = collider->GetOwner();
						hitResults.emplace_back(hit);
						Debug::DrawTriangle(tri, Vec4::cyan);
					}
				}
			}
		}
		// 動的メッシュ
		else {
		}
	};

	// 動的オブジェクトの処理
	for (int i : dynamicOverlaps) {
		ColliderComponent* collider = mColliderComponents[i];
		processRaycastOnMesh(collider, false);
	}
	// 静的オブジェクトの処理
	for (int i : staticOverlaps) {
		ColliderComponent* collider = mColliderComponents[i];
		processRaycastOnMesh(collider, true);
	}

	// 衝突結果を距離順にソート
	std::ranges::sort(
		hitResults, [](const HitResult& a, const HitResult& b) {
			return a.dist < b.dist;
		}
	);
	return hitResults;
}

bool PhysicsEngine::IntersectAABBWithTriangle(
	const AABB& aabb, const Triangle& triangle, HitResult& outHit
) {
	// AABBの中心と半径を取得
	Vec3 aabbCenter   = aabb.GetCenter();
	Vec3 aabbHalfSize = aabb.GetHalfSize();

	// 三角形の頂点をAABBのローカル座標系に変換
	Vec3 v0 = triangle.v0 - aabbCenter;
	Vec3 v1 = triangle.v1 - aabbCenter;
	Vec3 v2 = triangle.v2 - aabbCenter;

	// AABBの軸
	Vec3 axes[] = {
		Vec3::right, Vec3::up, Vec3::forward
	};

	// テスト1: AABBの各軸に対する投影
	for (int i = 0; i < 3; ++i) {
		float r    = aabbHalfSize[i];
		float p0   = v0[i];
		float p1   = v1[i];
		float p2   = v2[i];
		float minP = (std::min)({p0, p1, p2});
		float maxP = (std::max)({p0, p1, p2});
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
		v0 - v2
	};

	for (const Vec3& edge : edges) {
		for (const Vec3& a : axes) {
			Vec3 testAxis = edge.Cross(a);
			if (!testAxis.IsZero()) {
				bool overlap =
					OverlapOnAxis(aabbHalfSize, v0, v1, v2, testAxis);
				if (!overlap) {
					return false; // 分離軸発見
				}
			}
		}
	}

	// よくここまで来たな...喜べ衝突だ!
	outHit.isHit     = true;
	outHit.hitNormal = normal;

	// 衝突点の計算
	const Vec3 closestPoint = ClosestPointOnTriangleToPoint(
		Vec3::zero, triangle);
	outHit.hitPos = aabbCenter + closestPoint; // AABBのローカル座標からワールド座標へ

	Debug::DrawRay(
		outHit.hitPos, normal, Vec4::magenta
	);

	// 深さの計算 (AABB表面との最近距離)
	Vec3 penetration = ClosestPointOnAABBToPoint(closestPoint, aabb) -
		closestPoint;
	outHit.depth = penetration.Length();

	// その他の情報を設定
	outHit.dist      = outHit.depth; // 距離 = 深さとして扱う
	outHit.hitEntity = nullptr;      // 必要に応じて設定

	return true;
}

Vec3 PhysicsEngine::ClosestPointOnTriangleToPoint(
	const Vec3& point, const Triangle& triangle) {
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
	float s   = b * e - c * d;
	float t   = b * d - a * e;

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

Vec3 PhysicsEngine::ClosestPointOnAABBToPoint(const Vec3& point,
                                              const AABB& aabb) {
	Vec3 closestPoint = point;

	Vec3 min = aabb.GetCenter() - aabb.GetHalfSize();
	Vec3 max = aabb.GetCenter() + aabb.GetHalfSize();

	// 各軸でクランプ
	closestPoint.x = std::clamp(closestPoint.x, min.x, max.x);
	closestPoint.y = std::clamp(closestPoint.y, min.y, max.y);
	closestPoint.z = std::clamp(closestPoint.z, min.z, max.z);

	return closestPoint;
}

void PhysicsEngine::UpdateBVH() {
	for (auto* collider : mColliderComponents) {
		if (!collider) {
			break;
		}

		// 動的コライダーの更新処理
		if (collider->IsDynamic()) {
			AABB aabb = collider->GetBoundingBox();
			auto it   = mColliderNodeIds.find(collider);
			if (it != mColliderNodeIds.end()) {
				int nodeId = it->second;
				// BVHからオブジェクトを削除
				mDynamicBVH.RemoveObject(nodeId);
				// BVHにオブジェクトを再登録
				nodeId = mDynamicBVH.InsertObject(aabb, nodeId);
				mColliderNodeIds[collider] = nodeId;
			}
		}
	}

	mStaticBVH.DrawBvh(Vec4::magenta);

	for (const auto& [collider, nodeId] : mColliderNodeIds) {
		AABB nodeAABB = {Vec3::zero, Vec3::zero};
		nodeAABB      = mStaticMeshes[collider].localBVH.GetNodeAABB(nodeId);
		Debug::DrawBox(nodeAABB.GetCenter(), Quaternion::identity,
		               nodeAABB.GetSize(), Vec4::yellow);
	}
}

PhysicsEngine::StaticMeshData::StaticMeshData() {
}
