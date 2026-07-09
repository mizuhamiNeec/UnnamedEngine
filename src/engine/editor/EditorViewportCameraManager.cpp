#ifdef _DEBUG
#include "EditorViewportCameraManager.h"

#include <algorithm>

#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/CameraComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/world/EditorWorld.h"

namespace Unnamed {
	namespace {
		float ResolveAspectRatio(
			const Render::SceneViewRenderMode& sceneViewMode
		) {
			switch (sceneViewMode.mode) {
				case Render::SCENE_RENDER_MODE::FIXED_ASPECT_16X9
				: return 16.0f / 9.0f;
				case Render::SCENE_RENDER_MODE::FIXED_ASPECT_4X3
				: return 4.0f / 3.0f;
				case Render::SCENE_RENDER_MODE::HD_720P
				: return 1280.0f / 720.0f;
				case Render::SCENE_RENDER_MODE::FHD_1080P
				: return 1920.0f / 1080.0f;
				case Render::SCENE_RENDER_MODE::UHD_4K
				: return 3840.0f / 2160.0f;
				case Render::SCENE_RENDER_MODE::FIT_VIEWPORT:
				default: {
					const float width = static_cast<float>(std::max(
						1u, sceneViewMode.viewportPanelWidth
					));
					const float height = static_cast<float>(std::max(
						1u, sceneViewMode.viewportPanelHeight
					));
					return width / height;
				}
			}
		}

		Entity* FindFirstActiveGameplayCameraEntity(Scene* scene) {
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
			Scene* scene, const uint64_t entityGuid
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
								ResolveAspectRatio(sceneViewMode),
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
				*cameraEntity, ResolveAspectRatio(sceneViewMode), gameplay
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

		Scene* scene = editorWorld.GetActiveScene();
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
		camera->SetAspectRatio(ResolveAspectRatio(sceneViewMode));
	}
}

#endif
