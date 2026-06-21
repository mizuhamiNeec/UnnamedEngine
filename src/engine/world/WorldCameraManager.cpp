#include "WorldCameraManager.h"

#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/CameraComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		Entity* FindCameraEntityByGuid(
			Scene* scene, const uint64_t entityGuid
		) {
			if (!scene || entityGuid == 0) {
				return nullptr;
			}
			for (const auto& entity : scene->GetEntities()) {
				if (!entity || entity->GetGuid() != entityGuid || !entity->
				    IsActive()) {
					continue;
				}
				const auto* camera = entity->GetComponent<CameraComponent>();
				if (!camera || !camera->IsActive() || !camera->
				    IsCameraActive()) {
					continue;
				}
				return entity.get();
			}
			return nullptr;
		}

		Entity* FindFirstActiveCameraEntity(Scene* scene) {
			if (!scene) {
				return nullptr;
			}
			for (const auto& entity : scene->GetEntities()) {
				if (!entity || !entity->IsActive()) {
					continue;
				}
				const auto* camera = entity->GetComponent<CameraComponent>();
				if (!camera || !camera->IsActive() || !camera->
				    IsCameraActive()) {
					continue;
				}
				return entity.get();
			}
			return nullptr;
		}

		bool BuildCameraInput(
			Entity&                    entity, const float aspect,
			Render::RenderCameraInput& outCamera
		) {
			auto* camera = entity.GetComponent<CameraComponent>();
			if (!camera || !camera->IsActive() || !camera->IsCameraActive()) {
				return false;
			}
			camera->SetAspectRatio(aspect);
			return camera->BuildCameraInput(outCamera);
		}
	}

	WorldCameraManager::WorldCameraManager(World* world) : mWorld(world) {
	}

	void WorldCameraManager::SetWorld(World* world) noexcept {
		mWorld = world;
	}

	bool WorldCameraManager::SetCurrentCamera(const uint64_t entityGuid) {
		if (!mWorld || entityGuid == 0) {
			return false;
		}

		Scene* scene = mWorld->GetScenePtr();
		if (!scene) {
			return false;
		}

		if (!FindCameraEntityByGuid(scene, entityGuid)) {
			return false;
		}

		mCurrentCameraGuid = entityGuid;
		return true;
	}

	void WorldCameraManager::ClearCurrentCamera() {
		mCurrentCameraGuid = 0;
		ResetResolvedInfo();
	}

	uint64_t WorldCameraManager::GetCurrentCameraGuid() const noexcept {
		return mCurrentCameraGuid;
	}

	bool WorldCameraManager::HasCurrentCamera() const noexcept {
		return mCurrentCameraGuid != 0;
	}

	bool WorldCameraManager::ResolveForRender(
		const float aspect, Render::RenderCameraInput& outCamera
	) {
		outCamera = {};
		if (!mWorld) {
			ResetResolvedInfo();
			return false;
		}

		Scene* scene = mWorld->GetScenePtr();
		if (!scene) {
			ResetResolvedInfo();
			return false;
		}

		Entity* cameraEntity = nullptr;
		if (HasCurrentCamera()) {
			cameraEntity = FindCameraEntityByGuid(scene, mCurrentCameraGuid);
		}
		if (!cameraEntity) {
			cameraEntity = FindFirstActiveCameraEntity(scene);
		}
		if (!cameraEntity) {
			ResetResolvedInfo();
			return false;
		}

		if (!BuildCameraInput(*cameraEntity, aspect, outCamera)) {
			ResetResolvedInfo();
			return false;
		}

		mLastResolvedInfo.entityGuid = cameraEntity->GetGuid();
		mLastResolvedInfo.camera     = outCamera;
		mLastResolvedInfo.valid      = true;
		return true;
	}

	WorldCameraManager::CurrentCameraInfo
	WorldCameraManager::GetCurrentCameraInfo() const noexcept {
		return mLastResolvedInfo;
	}

	void WorldCameraManager::ResetResolvedInfo() noexcept {
		mLastResolvedInfo = {};
	}
}
