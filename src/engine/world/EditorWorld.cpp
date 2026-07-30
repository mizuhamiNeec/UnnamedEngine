#include "EditorWorld.h"

#include <chrono>

#include "engine/game/IGameWorldFactory.h"
#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/Scene.h"
#include "engine/scene/SceneSerializer.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/editor/EditorCameraComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "EditorWorld";

	void EditorWorld::Initialize() {
		World::Initialize();

		if (!mEditorEntity) {
			mEditorEntity = std::make_unique<Entity>(
				"__EditorCameraEntity", mGuidGenerator.Alloc(), true
			);
			auto* transform = mEditorEntity->AddComponent<TransformComponent>();
			transform->SetPosition(Math::HtoM({0.0f, 256.0f, -256.0f}));
			transform->SetRotation(
				Quaternion::EulerDegrees(Vec3::right * 15.0f)
			);
			mEditorEntity->AddComponent<EditorCameraComponent>();
		}
	}

	void EditorWorld::FixedTick(const float fixedDeltaTime) {
		if (IsPlaying()) {
			if (mPlayWorld) {
				mPlayWorld->FixedTick(fixedDeltaTime);
			}
		}
	}

	void EditorWorld::FrameInputTick(const float frameDeltaTime) {
		if (IsPlaying()) {
			if (mPlayWorld) {
				mPlayWorld->FrameInputTick(frameDeltaTime);
			}
		}
	}

	void EditorWorld::RenderTick(
		const float renderDeltaTime,
		const float interpolationAlpha
	) {
		(void)interpolationAlpha;
		if (IsPlaying()) {
			if (mPlayWorld) {
				mPlayWorld->RenderTick(renderDeltaTime, interpolationAlpha);
			}
			return;
		}

		// 編集モードでは見た目更新を描画フレームに同期する。
		if (mEditorEntity && mEditorEntity->IsActive()) {
			mEditorEntity->OnEditorTick(renderDeltaTime);
		}
		World::EditorTick(renderDeltaTime);
	}

	void EditorWorld::StartPlayInEditor() {
		if (mPlayWorld || !mScene) {
			return;
		}
		if (!mPlayWorldFactory) {
			Warning(kChannel, "PIE world factory is null.");
			return;
		}

		const auto totalStart = std::chrono::steady_clock::now();
		auto playWorld = mPlayWorldFactory->CreatePlayWorld(GetServices());
		if (!playWorld) {
			Warning(kChannel, "Failed to create PIE world from factory.");
			return;
		}
		// 念のため、WorldServices は常に最新値を上書きします。
		playWorld->SetServices(GetServices());
		const auto worldInitStart = std::chrono::steady_clock::now();
		playWorld->Initialize();
		const auto worldInitEnd = std::chrono::steady_clock::now();

		auto playScene = std::make_unique<Scene>();
		playScene->SetWorld(playWorld.get());
		GuidGenerator cloneGuid;
		const auto    cloneStart = std::chrono::steady_clock::now();
		if (!SceneSerializer::CloneScene(*mScene, *playScene, cloneGuid)) {
			return;
		}
		const auto cloneEnd = std::chrono::steady_clock::now();

		const auto setSceneStart = std::chrono::steady_clock::now();
		playWorld->SetScene(std::move(playScene));
		const auto setSceneEnd = std::chrono::steady_clock::now();
		mPlayWorld             = std::move(playWorld);

		DevMsg(
			kChannel,
			"StartPlayInEditor timing: worldInit={}ms clone={}ms setScene={}ms total={}ms",
			std::chrono::duration_cast<std::chrono::milliseconds>(
				worldInitEnd - worldInitStart
			).count(),
			std::chrono::duration_cast<std::chrono::milliseconds>(
				cloneEnd - cloneStart
			).count(),
			std::chrono::duration_cast<std::chrono::milliseconds>(
				setSceneEnd - setSceneStart
			).count(),
			std::chrono::duration_cast<std::chrono::milliseconds>(
				setSceneEnd - totalStart
			).count()
		);
	}

	void EditorWorld::StopPlayInEditor() {
		if (!mPlayWorld) {
			return;
		}
		mPlayWorld->Shutdown();
		mPlayWorld.reset();
		if (auto* input = GetInputSystem()) {
			input->SetMouseCursorLocked(false);
			input->SetMouseCursorVisible(true);
			input->ClearMouseCursorLockAnchor();
		}
	}

	void EditorWorld::SetPlayWorldFactory(
		IGameWorldFactory* const factory
	) noexcept {
		mPlayWorldFactory = factory;
	}

	void EditorWorld::FillRenderFrameInputs(
		Render::RenderFrameInputs&  inputs,
		Render::RenderFrameContext& frameContext,
		AssetManager&               assetManager,
		const bool                  enableUiInput
	) {
		if (mPlayWorld) {
			mPlayWorld->FillRenderFrameInputs(
				inputs, frameContext, assetManager, enableUiInput
			);
			return;
		}

		World::FillRenderFrameInputs(
			inputs, frameContext, assetManager, enableUiInput
		);

		Render::RenderViewInput* primarySceneView = nullptr;
		for (auto& view : inputs.views) {
			if (view.type == Render::RENDER_VIEW_TYPE::SCENE) {
				primarySceneView = &view;
				break;
			}
		}

		if (
			primarySceneView &&
			!primarySceneView->camera.valid &&
			mEditorEntity &&
			mEditorEntity->IsActive()
		) {
			if (
				const auto* camera =
					mEditorEntity->GetComponent<EditorCameraComponent>();
				camera && camera->IsActive()
			) {
				camera->BuildCameraInput(primarySceneView->camera);
			}
		}
	}

	bool EditorWorld::IsGameSimulationEnabled() const noexcept {
		return false;
	}

	World* EditorWorld::GetSimulationWorld() noexcept {
		return mPlayWorld ? mPlayWorld.get() : this;
	}

	World* EditorWorld::GetRuntimeSceneWorld() {
		return mPlayWorld ? mPlayWorld.get() : this;
	}

	const World* EditorWorld::GetRuntimeSceneWorld() const {
		return mPlayWorld ? static_cast<const World*>(mPlayWorld.get()) : this;
	}

	Scene* EditorWorld::GetActiveScene() {
		World* runtimeWorld = GetRuntimeSceneWorld();
		return runtimeWorld ? &runtimeWorld->GetScene() : nullptr;
	}

	const Scene* EditorWorld::GetActiveScene() const {
		const World* runtimeWorld = GetRuntimeSceneWorld();
		return runtimeWorld ? &runtimeWorld->GetScene() : nullptr;
	}

	EditorCameraComponent* EditorWorld::GetEditorCamera() {
		if (!mEditorEntity || !mEditorEntity->IsActive()) {
			return nullptr;
		}
		auto* camera = mEditorEntity->GetComponent<EditorCameraComponent>();
		if (!camera || !camera->IsActive()) {
			return nullptr;
		}
		return camera;
	}

	const EditorCameraComponent* EditorWorld::GetEditorCamera() const {
		if (!mEditorEntity || !mEditorEntity->IsActive()) {
			return nullptr;
		}
		const auto* camera =
			mEditorEntity->GetComponent<EditorCameraComponent>();
		if (!camera || !camera->IsActive()) {
			return nullptr;
		}
		return camera;
	}

	bool EditorWorld::BuildEditorCameraMatrices(
		const Render::SceneViewRenderMode& request,
		Mat4&                              outView,
		Mat4&                              outProj
	) {
		UpdateEditorCameraAspectIfNeeded(request);
		if (!mEditorEntity || !mEditorEntity->IsActive()) {
			return false;
		}

		const auto* camera = mEditorEntity->GetComponent<
			EditorCameraComponent>();
		if (!camera || !camera->IsActive()) {
			return false;
		}
		return camera->BuildViewProjectionMatrices(outView, outProj);
	}

	void EditorWorld::SetEditorCameraLookEnabled(const bool enabled) {
		auto* camera = GetEditorCamera();
		if (!camera) {
			return;
		}
		camera->SetLookEnabled(enabled);
	}

	bool EditorWorld::BuildUiInputCamera(
		Render::RenderCameraInput& outCamera
	) const {
		outCamera = {};
		if (!mEditorEntity || !mEditorEntity->IsActive()) {
			return false;
		}

		const auto* camera =
			mEditorEntity->GetComponent<EditorCameraComponent>();
		if (!camera || !camera->IsActive()) {
			return false;
		}

		camera->BuildCameraInput(outCamera);
		return outCamera.valid;
	}

	void EditorWorld::UpdateEditorCameraAspectIfNeeded(
		const Render::SceneViewRenderMode& request
	) {
		if (!mEditorEntity || !mEditorEntity->IsActive()) {
			return;
		}

		auto* camera = mEditorEntity->GetComponent<EditorCameraComponent>();
		if (!camera || !camera->IsActive()) {
			return;
		}

		bool shouldUpdate = false;
		switch (request.mode) {
			case Render::SCENE_RENDER_MODE::FIT_VIEWPORT: {
				shouldUpdate =
					request.mode != mLastAspectMode ||
					request.viewportPanelWidth != mLastAspectViewportWidth ||
					request.viewportPanelHeight != mLastAspectViewportHeight;
				break;
			}
			case Render::SCENE_RENDER_MODE::FIXED_ASPECT_16X9:
			case Render::SCENE_RENDER_MODE::FIXED_ASPECT_4X3:
			case Render::SCENE_RENDER_MODE::HD_720P:
			case Render::SCENE_RENDER_MODE::FHD_1080P:
			case Render::SCENE_RENDER_MODE::UHD_4K: {
				shouldUpdate = request.mode != mLastAspectMode;
				break;
			}
			default: break;
		}

		if (shouldUpdate) {
			camera->
				SetAspectRatio(Render::ResolveSceneViewAspectRatio(request));
		}

		mLastAspectMode           = request.mode;
		mLastAspectViewportWidth  = request.viewportPanelWidth;
		mLastAspectViewportHeight = request.viewportPanelHeight;
	}
}
