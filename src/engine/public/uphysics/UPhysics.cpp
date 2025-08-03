#include <pch.h>
#include <vector>

#include <engine/public/Camera/CameraManager.h>
#include <engine/public/Components/Camera/CameraComponent.h>
#include <engine/public/Components/ColliderComponent/MeshColliderComponent.h>
#include <engine/public/Debug/Debug.h>
#include <engine/public/ResourceSystem/Mesh/StaticMesh.h>
#include <engine/public/subsystem/console/Log.h>
#include <engine/public/uphysics/BoxCast.h>
#include <engine/public/uphysics/Primitives.h>
#include <engine/public/uphysics/RayCast.h>
#include <engine/public/uphysics/UPhysics.h>

namespace UPhysics {
	void Engine::Init() {
		// なんかする
	}

	void Engine::Update(float) const {
#ifdef _DEBUG
		const auto camera = CameraManager::GetActiveCamera();
		if (camera) {
			Mat4 invView = camera->GetViewMat().Inverse();
			Vec3 start   = invView.GetTranslate();
			Vec3 dir     = invView.GetForward();

			dir.Normalize();
			const Ray ray = {
				.start = start,
				.dir = dir,
				.invDir = 1.0f / dir,
				.tMin = 0.0f,
				.tMax = 1e30f
			};

			Debug::DrawAxis(
				start,
				Quaternion::identity
			);

			Hit hit;
			if (RayCast(ray, &hit)) {
				Debug::DrawRay(
					start,
					dir * hit.t,
					Vec4::blue
				);
				Debug::DrawAxis(
					hit.pos,
					Quaternion::identity
				);
				Debug::DrawRay(
					hit.pos,
					hit.normal,
					Vec4::magenta
				);
			}

			const Box box = {
				.center = start,
				.half = Vec3::one * 0.5f
			};
			if (BoxCast(box, dir, 65536.0f, &hit)) {
				ImGui::Begin("Hit");
				ImGui::End();
				Debug::DrawBox(
					hit.pos,
					Quaternion::identity,
					box.half * 2.0f,
					Vec4::blue
				);
				Debug::DrawBox(
					box.center + dir * hit.t,
					Quaternion::identity,
					box.half * 2.0f,
					Vec4::green
				);
				Debug::DrawAxis(
					hit.pos,
					Quaternion::identity
				);
				Debug::DrawRay(
					hit.pos,
					hit.normal,
					Vec4::magenta
				);
			}
		}
#endif
	}

	/// @brief エンティティを登録する関数
	/// @details メッシュコライダーを持ったエンティティを登録します
	/// @param entity 登録するエンティティ
	void Engine::RegisterEntity(Entity* entity) {
		if (!entity->HasComponent<MeshColliderComponent>()) {
			Warning(
				"UPhysics",
				"Entity '{}' does not have a MeshColliderComponent.",
				entity->GetName()
			);
			return;
		}

		auto meshCollider = entity->GetComponent<MeshColliderComponent>();
		if (!meshCollider) {
			Warning(
				"UPhysics",
				"Entity '{}' MeshColliderComponent is null.",
				entity->GetName()
			);
			return;
		}

		const auto transform = meshCollider->GetOwner()->GetTransform();

		for (
			const auto& subMesh : meshCollider->GetStaticMesh()->GetSubMeshes()
		) {
			auto tris = subMesh->GetPolygons();


			std::vector<Triangle> triangles;

			// UPhysics::Triangleに変換 TODO: すべてのTriangleをUPhysics::Triangleに変更する
			for (auto tri : tris) {
				tri.v0 += transform->GetLocalPos();
				tri.v1 += transform->GetLocalPos();
				tri.v2 += transform->GetLocalPos();

				triangles.emplace_back(
					tri.v0, tri.v1, tri.v2
				);
			}

			// BVHを構築
			BVHBuilder            bvhBuilder;
			std::vector<FlatNode> nodes;
			std::vector<uint32_t> triIndices;

			size_t triStart = mTriangles.size();

			bvhBuilder.Build(triangles, nodes, triIndices);

			size_t triCount = triangles.size();

			// インデックスにグローバルオフセットを追加
			AddGlobalOffset(
				triIndices,
				static_cast<uint32_t>(mTriangles.size())
			);

			// メンバに突っ込む
			mBVHs.emplace_back(
				RegisteredBVH{
					.nodes = std::move(nodes),
					.triIndices = std::move(triIndices),
					.triStart = triStart,
					.triCount = triCount,
					.owner = entity
				}
			);
			mTriangles.insert(
				mTriangles.end(),
				triangles.begin(),
				triangles.end()
			);

			DevMsg(
				"UPhysics",
				"Registered entity '{}' with {} triangles.",
				subMesh->GetName(),
				subMesh->GetPolygons().size()
			);
		}
	}

	void Engine::UnregisterEntity(const Entity* entity) {
		if (mBVHs.empty()) {
			return;
		}

		struct DelRange {
			size_t start, count;
		};
		std::vector<DelRange> ranges;

		std::erase_if(
			mBVHs,
			[&](const RegisteredBVH& bvh) {
				if (bvh.owner != entity) {
					return false;
				}
				ranges.emplace_back(bvh.triStart, bvh.triCount);
				return true;
			}
		);

		if (ranges.empty()) {
			return;
		}

		std::ranges::sort(ranges, [](auto& a, auto& b) {
			return a.start > b.start;
		});

		for (auto& [start, count] : ranges) {
			mTriangles.erase(
				mTriangles.begin() + static_cast<long long>(start),
				mTriangles.begin() + static_cast<long long>(start + count)
			);

			for (auto& bvh : mBVHs) {
				if (bvh.triStart > start) {
					bvh.triStart -= count;
					for (uint32_t& id : bvh.triIndices) {
						if (id >= start) {
							id -= static_cast<uint32_t>(count);
						}
					}
				}
			}
		}
	}

	bool Engine::RayCast(const Ray& ray, Hit* outHit) const {
		UPhysics::RayCast cast;
		cast.start  = ray.start;
		cast.invDir = ray.invDir;
		return CastBVH(cast, ray.start, ray.dir, ray.tMax, outHit, mBVHs,
		               mTriangles);
	}

	bool Engine::BoxCast(
		const Box&  box,
		const Vec3& dir,
		const float length,
		Hit*        outHit
	) const {
		Vec3  dirN = dir;
		float len  = length;

		float dirLen = dirN.Length();
		if (dirLen > 1e-6f) {
			dirN /= dirLen;
			if (fabs(len - dirLen) < 1e-4f)
				len = dirLen;
		} else {
			return false; // ゼロ方向なら衝突無し
		}

		UPhysics::BoxCast caster;
		caster.box  = box;
		caster.half = box.half;

		return CastBVH(
			caster,
			box.center,
			dirN,
			len,
			outHit,
			mBVHs, mTriangles
		);
	}

	void Engine::AddGlobalOffset(
		std::vector<uint32_t>& indices,
		const uint32_t         base
	) {
		for (uint32_t& index : indices) {
			index += base;
		}
	}
}
