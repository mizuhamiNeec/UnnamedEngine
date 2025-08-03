#include <pch.h>
#include <engine/public/Physics/PhysicsEngine.h>

#include <type_traits>

#include "engine/public/Components/ColliderComponent/MeshColliderComponent.h"
#include "engine/public/Debug/Debug.h"
#include "engine/public/Entity/Entity.h"
#include "engine/public/OldConsole/Console.h"
#include "engine/public/ResourceSystem/Mesh/StaticMesh.h"

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
		const auto* transform = entity->GetTransform();

		// メッシュコライダーからすべてのサブメッシュを取得
		const auto& meshes = meshCollider->GetStaticMesh()->GetSubMeshes();

		if (isStatic) {
			// サブメッシュごとに独立したBVHを持つStaticMeshDataを作成
			StaticMeshData meshData;
			Mat4           worldMatrix = transform->GetWorldMat();

			// すべてのサブメッシュの三角形数を事前に計算してreserve
			size_t totalTriangles = 0;
			for (const auto& subMesh : meshes) {
				totalTriangles += subMesh->GetPolygons().size();
			}
			meshData.worldTriangles.reserve(totalTriangles);

			// サブメッシュごとのBVH情報を格納
			struct SubMeshBVHInfo {
				DynamicBVH bvh;
				AABB       bounds             = {Vec3::zero, Vec3::zero};
				size_t     startTriangleIndex = 0;
				size_t     triangleCount      = 0;

				SubMeshBVHInfo() = default;

				SubMeshBVHInfo(SubMeshBVHInfo&& other) noexcept
					: bvh(std::move(other.bvh)),
					  bounds(other.bounds),
					  startTriangleIndex(other.startTriangleIndex),
					  triangleCount(other.triangleCount) {
				}
			};
			std::vector<SubMeshBVHInfo> subMeshBVHs;
			subMeshBVHs.reserve(meshes.size());

			size_t globalTriangleIndex = 0;

			// 各サブメッシュを処理
			for (const auto& subMesh : meshes) {
				SubMeshBVHInfo subMeshInfo;
				subMeshInfo.bvh.Clear();
				subMeshInfo.startTriangleIndex = globalTriangleIndex;
				subMeshInfo.triangleCount      = subMesh->GetPolygons().size();

				std::vector<Triangle> subMeshTriangles;
				subMeshTriangles.reserve(subMeshInfo.triangleCount);

				// サブメッシュの三角形を処理
				for (size_t i = 0; i < subMesh->GetPolygons().size(); ++i) {
					// 三角形をワールド空間に変換
					Triangle worldTri = {Vec3::zero, Vec3::zero, Vec3::zero};
					worldTri.v0       = Mat4::Transform(
						subMesh->GetPolygons()[i].v0, worldMatrix);
					worldTri.v1 = Mat4::Transform(
						subMesh->GetPolygons()[i].v1, worldMatrix);
					worldTri.v2 = Mat4::Transform(
						subMesh->GetPolygons()[i].v2, worldMatrix);

					meshData.worldTriangles.emplace_back(worldTri);
					subMeshTriangles.emplace_back(worldTri);

					// サブメッシュのBVHに登録
					AABB triAABB = FromTriangle(worldTri);
					subMeshInfo.bvh.InsertObject(
						triAABB, static_cast<int>(globalTriangleIndex));
					globalTriangleIndex++;
				}

				// サブメッシュ全体のAABBを計算
				subMeshInfo.bounds = FromTriangles(subMeshTriangles);
				subMeshBVHs.emplace_back(std::move(subMeshInfo));
			}

			// 統合BVHも作成（後方互換性のため）
			meshData.localBVH.Clear();
			for (size_t i = 0; i < meshData.worldTriangles.size(); ++i) {
				AABB triAABB = FromTriangle(meshData.worldTriangles[i]);
				meshData.localBVH.InsertObject(triAABB, static_cast<int>(i));
			}

			meshData.worldBounds = FromTriangles(meshData.worldTriangles);

			// TODO: subMeshBVHsをmeshDataに保存する仕組みが必要
			// 現在はStaticMeshDataにこの情報を格納する場所がないため、
			// 一時的に既存の実装を使用
			mStaticMeshes.emplace(collider, std::move(meshData));
		} else {
			DynamicMeshData meshData;
			meshData.localTriangles.reserve(meshes.size());
			meshData.transform = transform;
			for (const auto& subMesh : meshes) {
				// サブメッシュのポリゴンをローカル空間のまま登録
				meshData.localTriangles.insert(
					meshData.localTriangles.end(),
					subMesh->GetPolygons().begin(),
					subMesh->GetPolygons().end()
				);
			}
			mDynamicMeshes.emplace(collider, std::move(meshData));
		}

		// if (isStatic) {
		// 	// 静的メッシュはワールド空間に変換して登録
		// 	StaticMeshData meshData;
		// 	meshData.worldTriangles.reserve(triangles.size());
		// 	Mat4 worldMatrix = transform->GetWorldMat();
		//
		// 	// ローカルBVHの構築
		// 	meshData.localBVH.Clear();
		// 	for (size_t i = 0; i < triangles.size(); ++i) {
		// 		// 三角形をワールド空間に変換
		// 		Triangle worldTri = {Vec3::zero, Vec3::zero, Vec3::zero};
		// 		worldTri.v0 = Mat4::Transform(triangles[i].v0, worldMatrix);
		// 		worldTri.v1 = Mat4::Transform(triangles[i].v1, worldMatrix);
		// 		worldTri.v2 = Mat4::Transform(triangles[i].v2, worldMatrix);
		// 		meshData.worldTriangles.emplace_back(worldTri);
		//
		// 		// 三角形のAABBを計算してローカルBVHに登録
		// 		AABB triAABB = FromTriangle(worldTri);
		// 		meshData.localBVH.InsertObject(triAABB, static_cast<int>(i));
		// 	}
		//
		// 	// メッシュ全体のAABBを計算
		// 	meshData.worldBounds = FromTriangles(meshData.worldTriangles);
		// 	mStaticMeshes.emplace(collider, std::move(meshData));
		// } else {
		// 	// 動的メッシュはローカルのまま登録
		// 	DynamicMeshData meshData;
		// 	meshData.localTriangles = triangles;
		// 	meshData.transform      = transform;
		// 	mDynamicMeshes.emplace(collider, std::move(meshData));
		// }
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

	// 衝突判定を行う（距離に応じてステップ数を調整）
	const int steps = std::min(4, static_cast<int>(distance / 0.5f) + 1);
	// 0.5単位ごとに1ステップ
	float dt = distance / steps;
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
				// 静的メッシュの場合
				auto staticMeshIt = mStaticMeshes.find(meshCollider);
				if (staticMeshIt != mStaticMeshes.end()) {
					const auto& staticMeshData = staticMeshIt->second;

					// 全ての三角形をチェック（確実性を重視）
					for (const auto& tri : staticMeshData.worldTriangles) {
						HitResult hr;
						if (IntersectAABBWithTriangle(currentBox, tri, hr)) {
							// 衝突があった場合、tの値をヒット距離として設定
							hr.dist = t;
							// TODO: より正確なヒット位置を計算する
							hr.hitPos    = curPos;
							hr.hitEntity = entity;

#ifdef _DEBUG
							Debug::DrawTriangle(tri, Vec4::cyan);
#endif
							hitResults.emplace_back(hr);
						}
					}
				}

				// 動的メッシュの場合
				auto dynamicMeshIt = mDynamicMeshes.find(meshCollider);
				if (dynamicMeshIt != mDynamicMeshes.end()) {
					const auto& dynamicMeshData = dynamicMeshIt->second;
					const Mat4  worldMatrix     = dynamicMeshData.transform->
						GetWorldMat();

					for (const auto& localTri : dynamicMeshData.
					     localTriangles) {
						// ローカル三角形をワールド空間に変換
						Triangle worldTri(
							Mat4::Transform(localTri.v0, worldMatrix),
							Mat4::Transform(localTri.v1, worldMatrix),
							Mat4::Transform(localTri.v2, worldMatrix)
						);

						HitResult hr;
						if (IntersectAABBWithTriangle(
							currentBox, worldTri, hr)) {
							// 衝突があった場合、tの値をヒット距離として設定
							hr.dist = t;
							// TODO: より正確なヒット位置を計算する
							hr.hitPos    = curPos;
							hr.hitEntity = entity;

#ifdef _DEBUG
							Debug::DrawTriangle(worldTri, Vec4::cyan);
#endif
							hitResults.emplace_back(hr);
						}
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

		auto meshCollider = dynamic_cast<MeshColliderComponent*>(collider);
		if (!meshCollider) {
			return;
		}

		// 静的メッシュ
		if (isStatic) {
			auto staticMeshIt = mStaticMeshes.find(meshCollider);
			if (staticMeshIt != mStaticMeshes.end()) {
				const auto& staticMeshData = staticMeshIt->second;

				// ローカルBVHは使わず、全三角形をチェック（確実性を重視）
				// ただし、最初のヒットが見つかったら早期終了のオプションを追加
				float     closestHitDistance = distance + 1.0f;
				HitResult closestHit;
				bool      foundHit = false;

				for (const auto& tri : staticMeshData.worldTriangles) {
					float time;
					if (Physics::RayIntersectsTriangle(start, dir, tri, time) &&
						time <= distance && time < closestHitDistance) {
						closestHitDistance   = time;
						closestHit.isHit     = true;
						closestHit.dist      = time;
						closestHit.hitPos    = start + dir * time;
						closestHit.hitNormal = tri.GetNormal();
						closestHit.hitEntity = collider->GetOwner();
						foundHit             = true;

#ifdef _DEBUG
						Debug::DrawTriangle(tri, Vec4::cyan);
#endif

						// 最初のヒットで満足する場合は早期終了
						// break; // 必要に応じてコメントアウト
					}
				}

				if (foundHit) {
					hitResults.emplace_back(closestHit);
				}
			}
		}
		// 動的メッシュ
		else {
			auto dynamicMeshIt = mDynamicMeshes.find(meshCollider);
			if (dynamicMeshIt != mDynamicMeshes.end()) {
				const auto& dynamicMeshData = dynamicMeshIt->second;
				const Mat4  worldMatrix     = dynamicMeshData.transform->
					GetWorldMat();

				for (const auto& localTri : dynamicMeshData.localTriangles) {
					// ローカル三角形をワールド空間に変換
					Triangle worldTri(
						Mat4::Transform(localTri.v0, worldMatrix),
						Mat4::Transform(localTri.v1, worldMatrix),
						Mat4::Transform(localTri.v2, worldMatrix)
					);

					float time;
					if (Physics::RayIntersectsTriangle(
							start, dir, worldTri, time) &&
						time <= distance) {
						HitResult hit;
						hit.isHit     = true;
						hit.dist      = time;
						hit.hitPos    = start + dir * time;
						hit.hitNormal = worldTri.GetNormal();
						hit.hitEntity = collider->GetOwner();
						hitResults.emplace_back(hit);
#ifdef _DEBUG
						Debug::DrawTriangle(worldTri, Vec4::cyan);
#endif
					}
				}
			}
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
	const Vec3 aabbCenter   = aabb.GetCenter();
	const Vec3 aabbHalfSize = aabb.GetHalfSize();

	// 三角形の頂点をAABBのローカル座標系に変換
	const Vec3 v0 = triangle.v0 - aabbCenter;
	const Vec3 v1 = triangle.v1 - aabbCenter;
	const Vec3 v2 = triangle.v2 - aabbCenter;

	// AABBの軸
	static const Vec3 axes[] = {
		Vec3::right, Vec3::up, Vec3::forward
	};

	// テスト1: AABBの各軸に対する投影
	for (int i = 0; i < 3; ++i) {
		const float r  = aabbHalfSize[i];
		const float p0 = v0[i];
		const float p1 = v1[i];
		const float p2 = v2[i];
		// 初期化リストを使わず直接比較
		const float minP = std::min(std::min(p0, p1), p2);
		const float maxP = std::max(std::max(p0, p1), p2);
		if (minP > r || maxP < -r) {
			return false; // 分離軸発見
		}
	}

	// --- 以降は元のコードと同じ ---
	Vec3 normal = triangle.GetNormal();
	if (!normal.IsZero()) {
		if (!OverlapOnAxis(aabbHalfSize, v0, v1, v2, normal)) {
			return false;
		}
	}
	const Vec3 edges[3] = {v1 - v0, v2 - v1, v0 - v2};
	for (int ei = 0; ei < 3; ++ei) {
		const Vec3& edge = edges[ei];
		for (int ai = 0; ai < 3; ++ai) {
			const Vec3& a        = axes[ai];
			const Vec3  testAxis = edge.Cross(a);
			if (!testAxis.IsZero()) {
				if (!OverlapOnAxis(aabbHalfSize, v0, v1, v2, testAxis)) {
					return false;
				}
			}
		}
	}
	outHit.isHit            = true;
	outHit.hitNormal        = normal;
	const Vec3 closestPoint = ClosestPointOnTriangleToPoint(
		Vec3::zero, triangle);
	outHit.hitPos = aabbCenter + closestPoint;
	Debug::DrawRay(outHit.hitPos, normal, Vec4::magenta);
	const Vec3 penetration = ClosestPointOnAABBToPoint(closestPoint, aabb) -
		closestPoint;
	outHit.depth     = penetration.Length();
	outHit.dist      = outHit.depth;
	outHit.hitEntity = nullptr;
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

	for (const auto& [collider, meshData] : mStaticMeshes) {
		//meshData.localBVH.DrawBvh(Vec4::brown);
	}
}

PhysicsEngine::StaticMeshData::StaticMeshData() {
}
