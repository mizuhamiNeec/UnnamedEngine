#ifdef _DEBUG
#include "EditorViewportCameraManager.h"

#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/CameraComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/world/EditorWorld.h"

namespace Unnamed {
	namespace {
		Entity* FindFirstActiveGameplayCameraEntity(const Scene* scene) {
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

		Entity* FindGameplayCameraEntityByGuid(
			const Scene* scene, const uint64_t entityGuid
		) {
			if (!scene || entityGuid == 0) {
				return nullptr;
			}
			for (const auto& entity : scene->GetEntities()) {
				if (!entity || entity->GetGuid() != entityGuid) {
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

		bool BuildGameplayCameraInput(
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

	void EditorViewportCameraManager::SetPaneBinding(
		const std::string_view       viewKey,
		const ViewportCameraBinding& binding
	) {
		mPaneBindings[std::string(viewKey)] = binding;
	}

	ViewportCameraBinding EditorViewportCameraManager::GetPaneBinding(
		const std::string_view viewKey
	) const {
		if (const auto it = mPaneBindings.find(std::string(viewKey));
			it != mPaneBindings.end()) {
			return it->second;
		}
		return {};
	}

	EditorViewportCameraManager::ResolvedCamera
	EditorViewportCameraManager::ResolveViewCamera(
		EditorWorld&                       editorWorld,
		const std::string_view             viewKey,
		const Render::SceneViewRenderMode& sceneViewMode,
		const ViewportCameraBinding&       binding,
		const Render::RenderCameraInput*   fallbackCamera
	) {
		(void)viewKey;
		ResolvedCamera resolved = {};
		if (fallbackCamera && fallbackCamera->valid) {
			resolved.input = *fallbackCamera;
		}

		if (binding.kind == ViewportCameraBindingKind::EditorPerspective) {
			if (editorWorld.IsPlaying()) {
				if (Scene* scene = editorWorld.GetActiveScene()) {
					if (Entity* cameraEntity =
						FindFirstActiveGameplayCameraEntity(scene)) {
						Render::RenderCameraInput gameplay = {};
						if (
							BuildGameplayCameraInput(
								*cameraEntity,
								Render::ResolveSceneViewAspectRatio(
									sceneViewMode
								),
								gameplay
							)
						) {
							resolved.input          = gameplay;
							resolved.isOrthographic = false;
							return resolved;
						}
					}
				}
				if (fallbackCamera && fallbackCamera->valid) {
					resolved.input          = *fallbackCamera;
					resolved.isOrthographic = false;
					return resolved;
				}
			}
			Mat4 viewMat = Mat4::identity;
			Mat4 projMat = Mat4::identity;
			if (editorWorld.BuildEditorCameraMatrices(
				sceneViewMode, viewMat, projMat)) {
				resolved.input.valid     = true;
				resolved.input.view      = viewMat;
				resolved.input.proj      = projMat;
				resolved.input.viewProj  = viewMat * projMat;
				resolved.input.cameraPos = viewMat.Inverse().GetTranslate();
			}
			resolved.isOrthographic = false;
			return resolved;
		}

		Scene* scene = editorWorld.GetActiveScene();
		if (!scene) {
			return resolved;
		}

		Entity* cameraEntity = nullptr;
		if (binding.kind == ViewportCameraBindingKind::CameraEntity) {
			cameraEntity = FindGameplayCameraEntityByGuid(
				scene, binding.cameraEntityGuid
			);
		}
		if (!cameraEntity) {
			cameraEntity = FindFirstActiveGameplayCameraEntity(scene);
		}
		if (!cameraEntity) {
			return resolved;
		}

		Render::RenderCameraInput gameplay = {};
		if (
			BuildGameplayCameraInput(
				*cameraEntity,
				Render::ResolveSceneViewAspectRatio(sceneViewMode),
				gameplay
			)
		) {
			resolved.input = gameplay;
		}
		resolved.isOrthographic = false;
		return resolved;
	}

	void EditorViewportCameraManager::SyncGameplayCameraAspect(
		EditorWorld&                       editorWorld,
		const Render::SceneViewRenderMode& sceneViewMode,
		const ViewportCameraBinding&       binding
	) {
		if (binding.kind == ViewportCameraBindingKind::EditorPerspective) {
			return;
		}

		const Scene* scene = editorWorld.GetActiveScene();
		if (!scene) {
			return;
		}

		Entity* cameraEntity = nullptr;
		if (binding.kind == ViewportCameraBindingKind::CameraEntity) {
			cameraEntity = FindGameplayCameraEntityByGuid(
				scene, binding.cameraEntityGuid
			);
		}
		if (!cameraEntity) {
			cameraEntity = FindFirstActiveGameplayCameraEntity(scene);
		}
		if (!cameraEntity) {
			return;
		}

		auto* camera = cameraEntity->GetComponent<CameraComponent>();
		if (!camera || !camera->IsActive() || !camera->IsCameraActive()) {
			return;
		}
		camera->SetAspectRatio(
			Render::ResolveSceneViewAspectRatio(sceneViewMode)
		);
	}
}

#endif
