#include "World.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <chrono>
#include <cstdint>

#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/assets/types/MeshAssetData.h"

#include "engine/gui/UiCanvasRuntime.h"
#include "engine/gui/UiDrawCommand.h"
#include "engine/gui/UiRoot.h"
#include "engine/game/IDemoService.h"
#include "engine/physics/core/Physics.h"
#include "engine/profiler/Profiler.h"
#include "engine/render/frame/RenderFrameContext.h"
#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/rhi/Constants.h"
#include "engine/scene/Scene.h"
#include "engine/scene/SceneSerializer.h"
#include "engine/sequence/SequenceRuntime.h"
#include "engine/unnamed/framework/components/DirectionalLightComponent.h"
#include "engine/unnamed/framework/components/SkyLightComponent.h"
#include "engine/unnamed/framework/components/SkyboxComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/editor/EditorCameraComponent.h"
#include "engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h"
#include "engine/unnamed/framework/components/mesh/SkeletalMeshRendererComponent.h"
#include "engine/unnamed/framework/components/mesh/StaticMeshRendererComponent.h"
#include "engine/unnamed/framework/components/ui/NewUICanvas.h"
#include "engine/unnamed/framework/components/ui/UiCanvasComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/primitive/Primitives.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "core/filesystem/Path.h"

#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/subsystem/input/device/mouse/MouseDevice.h"
#include "engine/unnamed/ui/UIDrawCommandSprite.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "World";

	namespace {
		using TickGroup = BaseComponent::TICK_GROUP;

		enum class TICK_PHASE : uint8_t {
			PRE_PHYSICS  = 0,
			TICK         = 1,
			POST_PHYSICS = 2,
		};

		constexpr std::array kTickGroupOrder = {
			TickGroup::EARLY,
			TickGroup::KINEMATIC_SOURCE,
			TickGroup::COLLIDER_SYNC,
			TickGroup::GAMEPLAY,
			TickGroup::LATE,
		};

		constexpr std::array<std::array<const char*, 5>, 3>
		kPhaseGroupTimeSampleNames = {
			{
				{
					"World.Tick.PrePhysics.Early",
					"World.Tick.PrePhysics.KinematicSource",
					"World.Tick.PrePhysics.ColliderSync",
					"World.Tick.PrePhysics.Gameplay",
					"World.Tick.PrePhysics.Late"
				},
				{
					"World.Tick.OnTick.Early",
					"World.Tick.OnTick.KinematicSource",
					"World.Tick.OnTick.ColliderSync",
					"World.Tick.OnTick.Gameplay",
					"World.Tick.OnTick.Late"
				},
				{
					"World.Tick.PostPhysics.Early",
					"World.Tick.PostPhysics.KinematicSource",
					"World.Tick.PostPhysics.ColliderSync",
					"World.Tick.PostPhysics.Gameplay",
					"World.Tick.PostPhysics.Late"
				}
			}
		};

		constexpr std::array<std::array<const char*, 5>, 3>
		kPhaseGroupCountSampleNames = {
			{
				{
					"World.Tick.PrePhysics.Early.Count",
					"World.Tick.PrePhysics.KinematicSource.Count",
					"World.Tick.PrePhysics.ColliderSync.Count",
					"World.Tick.PrePhysics.Gameplay.Count",
					"World.Tick.PrePhysics.Late.Count"
				},
				{
					"World.Tick.OnTick.Early.Count",
					"World.Tick.OnTick.KinematicSource.Count",
					"World.Tick.OnTick.ColliderSync.Count",
					"World.Tick.OnTick.Gameplay.Count",
					"World.Tick.OnTick.Late.Count"
				},
				{
					"World.Tick.PostPhysics.Early.Count",
					"World.Tick.PostPhysics.KinematicSource.Count",
					"World.Tick.PostPhysics.ColliderSync.Count",
					"World.Tick.PostPhysics.Gameplay.Count",
					"World.Tick.PostPhysics.Late.Count"
				}
			}
		};

		[[nodiscard]] constexpr size_t ToPhaseIndex(const TICK_PHASE phase) {
			return static_cast<size_t>(phase);
		}

		[[nodiscard]] constexpr size_t ToGroupIndex(const TickGroup group) {
			return static_cast<size_t>(group);
		}

		/// @brief UiCanvasRuntimeEntryは、更新対象entityとそのtransform・legacy UI canvasへの非所有参照を保持します
		struct UiCanvasRuntimeEntry {
			Entity*             entity    = nullptr;
			TransformComponent* transform = nullptr;
			UiCanvasComponent*  canvas    = nullptr;
		};

		/// @brief NewUiCanvasRuntimeEntryは、更新対象のretained UI canvasへの非所有参照を保持します
		struct NewUiCanvasRuntimeEntry {
			NewUICanvas* canvas = nullptr;
		};

		[[nodiscard]] std::vector<Entity*> CollectActiveEntities(
			const Scene* scene
		) {
			std::vector<Entity*> activeEntities = {};
			if (!scene) {
				return activeEntities;
			}

			activeEntities.reserve(scene->GetEntities().size());
			for (const auto& entity : scene->GetEntities()) {
				if (!entity || !entity->IsActive()) {
					continue;
				}
				activeEntities.emplace_back(entity.get());
			}
			std::ranges::sort(
				activeEntities,
				[](const Entity* lhs, const Entity* rhs) {
					return lhs->GetGuid() < rhs->GetGuid();
				}
			);
			return activeEntities;
		}
	}

	World::~World() {
		Shutdown();
	}

	Scene& World::GetScene() {
		return *mScene;
	}

	const Scene& World::GetScene() const {
		return *mScene;
	}

	Scene* World::GetScenePtr() noexcept {
		return mScene.get();
	}

	const Scene* World::GetScenePtr() const noexcept {
		return mScene.get();
	}

	void World::Initialize() {
		// 物理エンジンの初期化
		mPhysicsEngine = std::make_unique<Physics::Engine>();
		mPhysicsEngine->Init();

		// カメラマネージャーの初期化
		mCameraManager.SetWorld(this);

		// シーンの初期化
		if (!mScene) {
			mScene = std::make_unique<Scene>();
		}
		mScene->SetWorld(this);
		for (const auto& entity : mScene->GetEntities()) {
			if (!entity) {
				continue;
			}
			entity->SetScene(mScene.get());
		}

		if (!mSequenceRuntime) {
			mSequenceRuntime = std::make_unique<SequenceRuntime>(this);
		} else {
			mSequenceRuntime->SetWorld(this);
		}
	}

	void World::Shutdown() {
		UnloadScene();
		mCameraManager.ClearCurrentCamera();
		mGameplayCueBus.Clear();
		if (mSequenceRuntime) {
			mSequenceRuntime->Clear();
		}
		if (mPhysicsEngine) {
			mPhysicsEngine->ClearStaticMeshes();
			mPhysicsEngine.reset();
		}
	}

	void World::Tick(const float unscaledDeltaTime, const float deltaTime) {
		// 互換ルート: 旧呼び出しは固定1ステップ+描画1ステップとして扱う。
		FrameInputTick(unscaledDeltaTime);
		FixedTick(deltaTime);
		RenderTick(unscaledDeltaTime, 0.0f);
	}

	void World::FixedTick(const float fixedDeltaTime) {
		// 更新対象の一覧を作る前に削除予約を確定する
		if (mScene) {
			mScene->ProcessPendingEntityDestruction();
		}

		mTime.fixedDeltaTime = fixedDeltaTime;
		mTime.timeSeconds    += fixedDeltaTime;
		++mTime.fixedTickCounter;

		if (!mScene) {
			return;
		}

		if (mSequenceRuntime) {
			mSequenceRuntime->AdvanceAndApplyPreSimulation(fixedDeltaTime);
		}

		// フェーズ間で対象と順序を揃えるため、この固定更新の一覧を固定する
		const std::vector<Entity*> activeEntities = CollectActiveEntities(
			mScene.get()
		);
		Profiler*  profiler      = mServices.profiler;
		const auto RunPhaseGroup =
			[&](
			const TICK_PHASE phase,
			const TickGroup  group
		) {
			const auto start                 = std::chrono::steady_clock::now();
			uint32_t   invokedComponentCount = 0;

			for (const Entity* entity : activeEntities) {
				switch (phase) {
					case TICK_PHASE::PRE_PHYSICS
					: invokedComponentCount += entity->PrePhysicsTick(
						  fixedDeltaTime,
						  group
					  );
						break;
					case TICK_PHASE::TICK
					: invokedComponentCount += entity->Tick(
						  fixedDeltaTime,
						  group
					  );
						break;
					case TICK_PHASE::POST_PHYSICS
					: invokedComponentCount += entity->PostPhysicsTick(
						  fixedDeltaTime,
						  group
					  );
						break;
				}
			}

			if (!profiler) {
				return;
			}

			const float elapsedMs = std::chrono::duration<float, std::milli>(
				std::chrono::steady_clock::now() - start
			).count();
			const size_t phaseIndex = ToPhaseIndex(phase);
			const size_t groupIndex = ToGroupIndex(group);
			profiler->AddSample(
				kPhaseGroupTimeSampleNames[phaseIndex][groupIndex],
				elapsedMs
			);
			profiler->AddSample(
				kPhaseGroupCountSampleNames[phaseIndex][groupIndex],
				static_cast<float>(invokedComponentCount)
			);
		};

		// 各フェーズで同じグループ順を守り、依存する更新順序を維持する
		for (const TickGroup group : kTickGroupOrder) {
			RunPhaseGroup(TICK_PHASE::PRE_PHYSICS, group);
		}
		for (const TickGroup group : kTickGroupOrder) {
			RunPhaseGroup(TICK_PHASE::TICK, group);
		}
		for (const TickGroup group : kTickGroupOrder) {
			RunPhaseGroup(TICK_PHASE::POST_PHYSICS, group);
		}

		if (mSequenceRuntime) {
			mSequenceRuntime->ApplyPostSimulation();
		}
	}

	void World::FrameInputTick(const float frameDeltaTime) {
		if (!mScene) {
			return;
		}

		const std::vector<Entity*> activeEntities = CollectActiveEntities(
			mScene.get()
		);
		for (const Entity* entity : activeEntities) {
			if (!entity) {
				continue;
			}
			(void)entity->FrameInputTick(frameDeltaTime);
		}
	}

	void World::RenderTick(
		const float renderDeltaTime,
		const float interpolationAlpha
	) {
		mTime.renderDeltaTime               = std::max(0.0f, renderDeltaTime);
		mTime.renderUnscaledDeltaTime       = std::max(0.0f, renderDeltaTime);
		ConsoleSystem* console              = mServices.console;
		const bool     interpolationEnabled =
			console ?
				console->GetConVarValueOr("cl_interpolate", true) :
				true;
		// cl_interpolateが無効のときは現在値をそのまま描画します。
		mTime.interpolationAlpha =
			interpolationEnabled ?
				std::clamp(interpolationAlpha, 0.0f, 1.0f) :
				1.0f;

		if (!mScene) {
			if (mPhysicsEngine) {
				mPhysicsEngine->EndFrame();
			}
			return;
		}

		const std::vector<Entity*> activeEntities = CollectActiveEntities(
			mScene.get()
		);

		for (const Entity* entity : activeEntities) {
			if (!entity) {
				continue;
			}
			(void)entity->RenderTick(
				mTime.renderDeltaTime,
				mTime.interpolationAlpha
			);
		}

		// Runtime UI は描画フレームティック側でのみ進める。
		const InputSystem* inputSystem = mServices.inputSystem;
		for (Entity* entity : activeEntities) {
			if (!entity) {
				continue;
			}
			auto* canvas = entity->GetComponent<UiCanvasComponent>();
			if (!canvas || !canvas->IsActive()) {
				continue;
			}
			if (!canvas->EnsureRuntimeLoaded()) {
				continue;
			}
			Gui::UiRoot* runtimeRoot = canvas->GetRuntimeRoot();
			if (!runtimeRoot) {
				continue;
			}

			Vec2 runtimeCanvasSize = canvas->GetPixelSize();
			if (canvas->GetSpaceMode() == UI_CANVAS_SPACE_MODE::SCREEN &&
			    inputSystem) {
				const Vec2 viewportSize = inputSystem->
					GetMouseClientViewportSize();
				if (viewportSize.x > 0.0f && viewportSize.y > 0.0f) {
					runtimeCanvasSize = viewportSize;
				}
			}
			runtimeRoot->SetRootSize(runtimeCanvasSize.x, runtimeCanvasSize.y);
			canvas->TickRuntime(mTime.renderDeltaTime);
		}

		for (
			const auto& [center, halfSize] :
			mPhysicsEngine->GetDebugBVHBoxes()
		) {
			GetDebugDraw().DrawBox(
				center,
				Quaternion::identity,
				halfSize,
				Vec4::orange
			);
		}

		for (const Entity* entity : activeEntities) {
			const auto transform = entity->GetComponent<TransformComponent>();
			if (!transform) {
				continue;
			}
			Mat4       worldMat = transform->RenderWorldMat();
			const Vec3 pos      = worldMat.GetTranslate();
			GetDebugDraw().DrawAxis(
				pos, transform->Right(), transform->Up(), transform->Forward()
			);
		}

		mPhysicsEngine->EndFrame();
	}

	void World::EditorTick(const float unscaledDeltaTime) {
		if (!mScene) {
			return;
		}

		// まずは削除予定のエンティティを削除
		mScene->ProcessPendingEntityDestruction();

		mTime.renderUnscaledDeltaTime = unscaledDeltaTime;

		if (mSequenceRuntime) {
			mSequenceRuntime->EditorTick(unscaledDeltaTime);
		}

		for (const auto& entity : mScene->GetEntities()) {
			if (!entity || !entity->IsActive()) {
				continue;
			}
			entity->OnEditorTick(unscaledDeltaTime);
		}
	}

	bool World::LoadSceneFromFile(Path path) {
		return LoadSceneFromFile(std::move(path), mSceneLoadOptions);
	}

	bool World::LoadSceneFromFile(
		Path path, const SceneLoadOptions& options
	) {
		if (path.IsEmpty()) {
			return false;
		}

		// GUIDが重複するシーン間遷移でも、旧シーンのDetachが新シーン登録を消さないよう
		// 先に旧シーンをアンロードしてから新シーンをロードします。
		UnloadScene();

		auto newScene = std::make_unique<Scene>();
		newScene->SetWorld(this);
		const bool ok = SceneSerializer::LoadFromFile(
			*newScene, path, mGuidGenerator, options
		);
		if (!ok) {
			return false;
		}

		SetScene(std::move(newScene));
		mLoadedScenePath = path.LexicallyNormal();
		OnSceneLoaded();

		Msg(
			kChannel, "Scene loaded successfully from file: {}",
			path
		);

		return true;
	}

	bool World::SaveSceneToFile(Path path) const {
		if (!mScene || path.IsEmpty()) {
			return false;
		}
		return SceneSerializer::SaveToFile(*mScene, path);
	}

	void World::UnloadScene() {
		if (!mScene) {
			return;
		}

		OnSceneUnloaded();
		mCameraManager.ClearCurrentCamera();
		if (mSequenceRuntime) {
			mSequenceRuntime->Clear();
		}
		mScene.reset();
		mLoadedScenePath = {};
	}

	void World::RequestSceneTransition(Path path) {
		path = path.IsEmpty() ? Path() : path.LexicallyNormal();
		if (path.IsEmpty()) {
			Warning(kChannel,
			        "Scene transition was ignored because path is empty.");
			return;
		}

		// 実行中のシーン更新中に即時ロードしないため、次の安全地点まで保留します。
		mPendingSceneTransitionPath = std::move(path);
		Msg(
			kChannel,
			"Queued scene transition to: {}",
			mPendingSceneTransitionPath
		);
	}

	void World::ProcessPendingSceneTransition() {
		if (mPendingSceneTransitionPath.IsEmpty()) {
			return;
		}

		// 再入時に同じ要求を処理しないよう、先に保留値を取り出します。
		const Path requestedPath    = std::move(mPendingSceneTransitionPath);
		mPendingSceneTransitionPath = {};

		if (LoadSceneFromFile(requestedPath)) {
			return;
		}

		Error(
			kChannel,
			"Scene transition failed: {}",
			requestedPath
		);
	}

	void World::FillRenderFrameInputs(
		Render::RenderFrameInputs&  inputs,
		Render::RenderFrameContext& frameContext,
		AssetManager&               assetManager,
		const bool                  enableUiInput
	) {
		frameContext.Reset();
		inputs.views.clear();

#ifdef _DEBUG
		mDebugDraw.FlushToRenderFrameInputs(inputs);
#endif

		if (!mScene) {
			return;
		}

		Render::RenderViewInput sceneView = {};
		bool                    skyLightResolved = false;
		sceneView.viewKey = "world.main";
		sceneView.type = Render::RENDER_VIEW_TYPE::SCENE;
		sceneView.output.sizeMode =
			Render::RENDER_VIEW_SIZE_MODE::MATCH_BACK_BUFFER;
		sceneView.output.presentToSwapChain = true;
		sceneView.output.clearSwapChainWhenNotPresenting = false;
		sceneView.sceneViewMode.mode = Render::SCENE_RENDER_MODE::FIT_VIEWPORT;
		sceneView.postFxPassOverrides.reserve(mPostFxPassOverrides.size());
		for (const auto& [passName, values] : mPostFxPassOverrides) {
			Render::PostFxPassOverride passOverride = {};
			passOverride.passName                   = passName;
			if (values.enabled.has_value()) {
				passOverride.hasEnabledOverride = true;
				passOverride.enabled            = *values.enabled;
			}
			passOverride.scalarParams = values.scalarParams;
			passOverride.colorParams  = values.colorParams;
			sceneView.postFxPassOverrides.emplace_back(std::move(passOverride));
		}

		std::vector<UiCanvasRuntimeEntry> uiCanvasEntries;
		uiCanvasEntries.reserve(mScene->GetEntities().size());
		std::vector<NewUiCanvasRuntimeEntry> newUiCanvasEntries;
		newUiCanvasEntries.reserve(mScene->GetEntities().size());
		InputSystem* inputSystem        = mServices.inputSystem;
		static bool  sTextWarningLogged = false;
		const Vec2   aspectViewportSize = inputSystem ?
			                                inputSystem->
			                                GetMouseClientViewportSize() :
			                                Vec2::zero;
		const float runtimeAspect = aspectViewportSize.y > 0.0f ?
			                            aspectViewportSize.x /
			                            aspectViewportSize.y :
			                            16.0f / 9.0f;
		mCameraManager.ResolveForRender(runtimeAspect, sceneView.camera);

		for (const auto& entity : mScene->GetEntities()) {
			if (!entity || !entity->IsActive()) {
				continue;
			}

			if (!sceneView.camera.valid) {
				const auto* editorCamera = entity->GetComponent<
					EditorCameraComponent>();
				if (editorCamera && editorCamera->IsActive()) {
					editorCamera->BuildCameraInput(sceneView.camera);
				}
			}

			if (entity->IsEditorOnly() || !entity->IsVisible()) {
				continue;
			}

			if (!sceneView.skybox.enabled) {
				const auto* skybox = entity->GetComponent<SkyboxComponent>();
				if (skybox && skybox->IsActive()) {
					const AssetID skyboxTextureAssetId =
						skybox->GetTextureAssetId();
					if (skyboxTextureAssetId != kInvalidAssetID) {
						sceneView.skybox.enabled = true;
						sceneView.skybox.textureAssetId = skyboxTextureAssetId;
						sceneView.skybox.intensity = skybox->GetIntensity();
					}
				}
			}

			if (!sceneView.directionalLight.enabled) {
				auto* directionalLight = entity->GetComponent<
					DirectionalLightComponent>();
				if (directionalLight && directionalLight->IsActive()) {
					(void)directionalLight->BuildLightInput(
						sceneView.directionalLight
					);
				}
			}

			if (!skyLightResolved) {
				auto* skyLight = entity->GetComponent<SkyLightComponent>();
				if (skyLight && skyLight->IsActive()) {
					skyLightResolved = skyLight->BuildLightInput(
						sceneView.environmentLight
					);
				}
			}

			auto* transform    = entity->GetComponent<TransformComponent>();
			auto* meshRenderer = entity->GetComponent<
				StaticMeshRendererComponent>();
			auto* skelRenderer = entity->GetComponent<
				SkeletalMeshRendererComponent>();
			auto* uiCanvas    = entity->GetComponent<UiCanvasComponent>();
			auto* newUiCanvas = entity->GetComponent<NewUICanvas>();
			if (newUiCanvas && newUiCanvas->IsActive()) {
				NewUiCanvasRuntimeEntry entry = {};
				entry.canvas                  = newUiCanvas;
				newUiCanvasEntries.emplace_back(entry);
			}
			if (!transform) {
				continue;
			}

			if (uiCanvas && uiCanvas->IsActive()) {
				UiCanvasRuntimeEntry entry = {};
				entry.entity               = entity.get();
				entry.transform            = transform;
				entry.canvas               = uiCanvas;
				uiCanvasEntries.emplace_back(entry);
			}

			if (meshRenderer && meshRenderer->IsActive()) {
				const AssetID meshAssetId = meshRenderer->ResolveMeshAsset(
					assetManager
				);
				if (meshAssetId != kInvalidAssetID) {
					meshRenderer->ResolveMaterialInstanceAssets(assetManager);

					const auto* meshAsset = assetManager.Get<MeshAssetData>(
						meshAssetId
					);
					const uint32_t materialIndex =
						meshAsset && !meshAsset->submeshes.empty() ?
							meshAsset->submeshes.front().materialIndex :
							0u;
					Render::VisibleRenderObject object = {};
					object.meshAssetId                 = meshAssetId;
					const auto& materialSlots          = meshRenderer->
						GetMaterialSlots();
					uint32_t maxSlotIndex = 0;
					for (const auto& slot : materialSlots) {
						maxSlotIndex = std::max(maxSlotIndex, slot.slotIndex);
					}
					if (!materialSlots.empty()) {
						object.materialInstanceIdsBySlot.resize(
							maxSlotIndex + 1,
							kInvalidAssetID
						);
						for (const auto& slot : materialSlots) {
							object.materialInstanceIdsBySlot[slot.slotIndex] =
								meshRenderer->GetMaterialInstanceAssetIdForSlot(
									slot.slotIndex
								);
						}
					}
					object.materialInstanceId = meshRenderer->
						ResolveMaterialInstanceAssetForMaterialIndex(
							assetManager,
							materialIndex
						);
					object.ownerEntityGuid = entity->GetGuid();
					object.world           = transform->RenderWorldMat();
					object.isSkinned       = false;
					sceneView.visibleObjects.emplace_back(object);
				}
			}

			if (skelRenderer && skelRenderer->IsActive()) {
				const AssetID meshAssetId = skelRenderer->GetMeshAssetId();
				if (meshAssetId == kInvalidAssetID) {
					continue;
				}

				Render::VisibleRenderObject object = {};
				object.meshAssetId                 = meshAssetId;

				const auto& materialSlots = skelRenderer->GetMaterialSlots();
				uint32_t    maxSlotIndex  = 0;
				for (const auto& slot : materialSlots) {
					maxSlotIndex = std::max(maxSlotIndex, slot.slotIndex);
				}
				if (!materialSlots.empty()) {
					object.materialInstanceIdsBySlot.resize(
						maxSlotIndex + 1,
						kInvalidAssetID
					);
					for (const auto& slot : materialSlots) {
						object.materialInstanceIdsBySlot[slot.slotIndex] =
							skelRenderer->GetMaterialInstanceAssetIdForSlot(
								slot.slotIndex
							);
					}
				}

				const auto* meshAsset = assetManager.Get<MeshAssetData>(
					meshAssetId
				);
				const uint32_t materialIndex =
					meshAsset && !meshAsset->submeshes.empty() ?
						meshAsset->submeshes.front().materialIndex :
						0u;
				object.materialInstanceId = skelRenderer->
					GetMaterialInstanceAssetIdForMaterialIndex(materialIndex);
				object.ownerEntityGuid   = entity->GetGuid();
				object.world             = transform->RenderWorldMat();
				object.isSkinned         = false;
				object.skeletonPaletteId = 0;

				const bool hasSkinning =
					meshAsset && meshAsset->hasSkinning && !meshAsset->skeleton.
					empty();
				if (hasSkinning) {
					object.isSkinned                     = true;
					Render::SkinningPaletteInput palette = {};
					palette.meshAssetId                  = meshAssetId;

					const uint32_t boneCount = std::min<uint32_t>(
						static_cast<uint32_t>(meshAsset->skeleton.size()),
						Rhi::SkinningPaletteConstants::kMaxBones
					);
					palette.boneMatrices.resize(boneCount, Mat4::identity);

					if (auto* anim = entity->GetComponent<
							SkeletalAnimationComponent>();
						anim && anim->IsActive()) {
						anim->BuildSkinningPalette(
							*meshAsset, palette.boneMatrices
						);
					} else {
						SkeletalAnimationComponent::BuildBindPosePalette(
							*meshAsset, palette.boneMatrices
						);
					}

					object.skeletonPaletteId = static_cast<uint32_t>(
						sceneView.skinningPalettes.size()
					);
					sceneView.skinningPalettes.emplace_back(std::move(palette));
				}

				sceneView.visibleObjects.emplace_back(object);
			}
		}

		for (UiCanvasRuntimeEntry& entry : uiCanvasEntries) {
			if (!entry.entity || !entry.transform || !entry.canvas) {
				continue;
			}
			if (!entry.canvas->EnsureRuntimeLoaded()) {
				continue;
			}

			Gui::UiRoot* runtimeRoot = entry.canvas->GetRuntimeRoot();
			if (!runtimeRoot) {
				continue;
			}

			const Vec2 pixelSize         = entry.canvas->GetPixelSize();
			Vec2       runtimeCanvasSize = pixelSize;
			if (
				entry.canvas->GetSpaceMode() == UI_CANVAS_SPACE_MODE::SCREEN &&
				inputSystem
			) {
				const Vec2 viewportSize = inputSystem->
					GetMouseClientViewportSize();
				if (viewportSize.x > 0.0f && viewportSize.y > 0.0f) {
					runtimeCanvasSize = viewportSize;
				}
			}
			runtimeRoot->SetRootSize(runtimeCanvasSize.x, runtimeCanvasSize.y);
			runtimeRoot->UpdateLayout();

			const bool canProcessInput =
				enableUiInput && entry.canvas->GetReceiveInput() &&
				inputSystem != nullptr;
			if (canProcessInput) {
				const Vec2 mousePos     = inputSystem->GetMouseClientPosition();
				const Vec2 viewportSize = inputSystem->
					GetMouseClientViewportSize();
				const bool leftDown    = inputSystem->IsMouseButtonDown(VM_1);
				const bool leftPressed = inputSystem->WasMouseButtonPressed(
					VM_1
				);
				const bool leftReleased = inputSystem->WasMouseButtonReleased(
					VM_1
				);

				if (entry.canvas->GetSpaceMode() ==
				    UI_CANVAS_SPACE_MODE::SCREEN) {
					runtimeRoot->ProcessMouse(
						mousePos.x,
						mousePos.y,
						leftDown,
						leftPressed,
						leftReleased
					);
				} else {
					Render::RenderCameraInput inputCamera = sceneView.camera;
					if (!inputCamera.valid) {
						Render::RenderCameraInput fallbackCamera = {};
						if (
							BuildUiInputCamera(fallbackCamera) &&
							fallbackCamera.valid
						) {
							inputCamera = fallbackCamera;
						}
					}

					bool processed = false;
					Ray  ray       = {};
					if (UiCanvasRuntime::BuildRayFromMouse(
						inputCamera, mousePos, viewportSize, ray
					)) {
						Vec3 axisRight      = Vec3::right;
						Vec3 axisUp         = Vec3::up;
						Mat4 transformWorld = entry.transform->RenderWorldMat();
						Vec3 center         = transformWorld.GetTranslate();

						if (
							entry.canvas->GetSpaceMode() ==
							UI_CANVAS_SPACE_MODE::WORLD_BILLBOARD
						) {
							const Mat4 cameraWorld = sceneView.camera.view.
								Inverse();
							axisRight = cameraWorld.GetRight().Normalized();
							axisUp    = cameraWorld.GetUp().Normalized();
						} else {
							axisRight = entry.
							            transform->RenderWorldMat().GetRight().
							            Normalized();
							axisUp = entry.transform->RenderWorldMat().GetUp().
							               Normalized();
						}

						float hitDistance = 0.0f;
						Vec2  localWorld  = Vec2::zero;
						if (
							UiCanvasRuntime::RayIntersectsUiPlane(
								ray,
								center,
								axisRight,
								axisUp,
								entry.canvas->GetWorldSize(),
								hitDistance,
								localWorld
							) &&
							!UiCanvasRuntime::RayHitsSceneGeometryBefore(
								*mScene,
								assetManager,
								ray,
								hitDistance,
								entry.entity->GetGuid()
							)
						) {
							const Vec2 localPixels =
								UiCanvasRuntime::WorldToUiPixels(
									localWorld,
									entry.canvas->GetWorldSize(),
									pixelSize
								);
							runtimeRoot->ProcessMouse(
								localPixels.x,
								localPixels.y,
								leftDown,
								leftPressed,
								leftReleased
							);
							processed = true;
						}
					}
					if (!processed) {
						runtimeRoot->ProcessMouse(
							-FLT_MAX,
							-FLT_MAX,
							leftDown,
							leftPressed,
							leftReleased
						);
					}
				}
			}

			std::vector<Gui::UiDrawCommand> drawCommands;
			runtimeRoot->BuildDrawCommands(drawCommands);

			const Vec2    worldSize        = entry.canvas->GetWorldSize();
			const int32_t canvasSort       = entry.canvas->GetSortKey();
			const bool    worldSpaceCanvas =
				entry.canvas->GetSpaceMode() != UI_CANVAS_SPACE_MODE::SCREEN;

			Render::RenderViewInput canvasSpriteView = {};
			std::string             canvasViewKey;
			if (worldSpaceCanvas) {
				canvasViewKey = std::string("ui.canvas.") +
				                std::to_string(entry.entity->GetGuid()) + "." +
				                std::to_string(entry.canvas->GetGuid());
				canvasSpriteView.viewKey = canvasViewKey;
				canvasSpriteView.type = Render::RENDER_VIEW_TYPE::SPRITE_ONLY;
				canvasSpriteView.output.sizeMode =
					Render::RENDER_VIEW_SIZE_MODE::FIXED;
				canvasSpriteView.output.width = static_cast<uint32_t>(std::max(
					1.0f, pixelSize.x
				));
				canvasSpriteView.output.height = static_cast<uint32_t>(std::max(
					1.0f, pixelSize.y
				));
			}

			for (size_t i = 0; i < drawCommands.size(); ++i) {
				const auto&   draw = drawCommands[i];
				const int32_t sort =
					canvasSort * 10000 + static_cast<int32_t>(i);

				if (draw.type == Gui::UI_DRAW_COMMAND_TYPE::RECT) {
					if (
						entry.canvas->GetSpaceMode() ==
						UI_CANVAS_SPACE_MODE::SCREEN
					) {
						sceneView.screenSprites.emplace_back(
							UiCanvasRuntime::BuildScreenSprite(draw.rect, sort)
						);
					} else {
						canvasSpriteView.screenSprites.emplace_back(
							UiCanvasRuntime::BuildScreenSprite(draw.rect, sort)
						);
					}
					continue;
				}

				if (draw.type == Gui::UI_DRAW_COMMAND_TYPE::IMAGE) {
					Render::ScreenSpriteInput sprite =
						UiCanvasRuntime::BuildScreenSprite(draw.image, sort);

					if (
						entry.canvas->GetSpaceMode() ==
						UI_CANVAS_SPACE_MODE::SCREEN
					) {
						sceneView.screenSprites.emplace_back(std::move(sprite));
					} else {
						canvasSpriteView.screenSprites.emplace_back(
							std::move(sprite));
					}
					continue;
				}

				if (!sTextWarningLogged) {
					DevMsg(
						kChannel,
						"UiDrawCommand TEXT is skipped in runtime pass."
					);
					sTextWarningLogged = true;
				}
			}

			if (!worldSpaceCanvas) {
				continue;
			}

			inputs.views.emplace_back(std::move(canvasSpriteView));

			Mat4       transformWorld = entry.transform->RenderWorldMat();
			const Vec3 center         = transformWorld.GetTranslate();
			if (
				entry.canvas->GetSpaceMode() ==
				UI_CANVAS_SPACE_MODE::WORLD_BILLBOARD
			) {
				Render::WorldBillboardInput billboard = {};
				billboard.texture.source              =
					Render::SPRITE_TEXTURE_SOURCE::VIEW_OUTPUT;
				billboard.texture.viewKey = canvasViewKey;
				billboard.worldPosition   = center;
				billboard.sizeWorld       = worldSize;
				billboard.color           = Vec4::one;
				billboard.rotationRad     = 0.0f;
				billboard.sortKey         = canvasSort;
				billboard.uvFlipY         = false;
				billboard.depthTest       =
					entry.canvas->GetBillboardDepthMode() ==
					UI_CANVAS_BILLBOARD_DEPTH_MODE::DEPTH_TEST;
				sceneView.worldBillboards.emplace_back(billboard);
			} else {
				Render::WorldSpriteInput sprite = {};
				sprite.texture.source           =
					Render::SPRITE_TEXTURE_SOURCE::VIEW_OUTPUT;
				sprite.texture.viewKey = canvasViewKey;
				sprite.worldPosition   = center;
				sprite.worldRight      = transformWorld.GetRight().Normalized();
				sprite.worldUp         = transformWorld.GetUp().Normalized();
				sprite.sizeWorld       = worldSize;
				sprite.color           = Vec4::one;
				sprite.rotationRad     = 0.0f;
				sprite.sortKey         = canvasSort;
				sprite.uvFlipY         = false;
				sceneView.worldSprites.emplace_back(sprite);
			}
		}

		size_t                       newUiOverlaySpriteCount = 0;
		UI::UIDrawCommandSpriteStats newUiSpriteStats        = {};
		size_t                       newUiRectCommandCount   = 0;
		size_t                       newUiTextCommandCount   = 0;
		for (size_t canvasIndex = 0; canvasIndex < newUiCanvasEntries.size();
		     ++canvasIndex) {
			const auto& entry = newUiCanvasEntries[canvasIndex];
			if (!entry.canvas) {
				continue;
			}

			const auto& commands = entry.canvas->GetDrawCommands();
			for (const UI::UIDrawCommand& command : commands) {
				if (command.type == UI::UIDrawCommandType::RECT) {
					++newUiRectCommandCount;
				} else if (command.type == UI::UIDrawCommandType::TEXT) {
					++newUiTextCommandCount;
				}
			}
			UI::UIFontAtlas* fontAtlas = entry.canvas->ResolveFontAtlas(
				assetManager
			);
			for (size_t commandIndex = 0; commandIndex < commands.size();
			     ++commandIndex) {
				const int32_t baseSortKey =
					static_cast<int32_t>(canvasIndex) * 100000 +
					static_cast<int32_t>(commandIndex) * 256;
				std::vector<Render::ScreenSpriteInput> commandSprites;
				UI::AppendDrawCommandScreenSprites(
					commands[commandIndex],
					baseSortKey,
					fontAtlas,
					commandSprites,
					&newUiSpriteStats
				);
				if (commandSprites.empty()) {
					continue;
				}
				newUiOverlaySpriteCount += commandSprites.size();
				frameContext.AddOverlaySprites(std::move(commandSprites));
			}
		}

		const auto& frameOverlaySprites =
			frameContext.GetOverlayData().screenSprites;
		const size_t sceneSpritesBeforeMerge = sceneView.screenSprites.size();
		if (!frameOverlaySprites.empty()) {
			sceneView.screenSprites.reserve(
				sceneView.screenSprites.size() + frameOverlaySprites.size()
			);
			sceneView.screenSprites.insert(
				sceneView.screenSprites.end(),
				frameOverlaySprites.begin(),
				frameOverlaySprites.end()
			);
		}
		static bool sLoggedNewUiPipelineSummary = false;
		if (!sLoggedNewUiPipelineSummary && newUiOverlaySpriteCount > 0) {
			DevMsg(
				"UI",
				"NewUI pipeline summary frame={}: commandGen(rect={}, text={}) -> overlayTransform(glyphSprites={}, skippedGlyphs={}, totalOverlaySprites={}) -> frameContextSubmit(overlaySprites={}) -> sceneMerge(before={}, added={}, after={}).",
				inputs.frameIndex,
				newUiRectCommandCount,
				newUiTextCommandCount,
				newUiSpriteStats.glyphSpriteCount,
				newUiSpriteStats.skippedGlyphCount,
				newUiOverlaySpriteCount,
				frameOverlaySprites.size(),
				sceneSpritesBeforeMerge,
				frameOverlaySprites.size(),
				sceneView.screenSprites.size()
			);
			sLoggedNewUiPipelineSummary = true;
		}

		if (!mDebugScreenSprites.empty()) {
			sceneView.screenSprites.reserve(
				sceneView.screenSprites.size() + mDebugScreenSprites.size()
			);
			for (auto& sprite : mDebugScreenSprites) {
				sceneView.screenSprites.emplace_back(std::move(sprite));
			}
			mDebugScreenSprites.clear();
		}

		inputs.views.emplace_back(std::move(sceneView));
	}

	bool World::IsGameSimulationEnabled() const noexcept {
		return true;
	}

	Path World::GetLoadedScenePath() const {
		return mLoadedScenePath;
	}

	uint64_t World::GetSceneGeneration() const noexcept {
		return mSceneGeneration;
	}

	World* World::GetSimulationWorld() noexcept {
		return this;
	}

	void World::SetLoadedScenePath(const Path& path) {
		mLoadedScenePath = path.IsEmpty() ? Path() : path.LexicallyNormal();
	}

	WorldCameraManager& World::GetCameraManager() noexcept {
		return mCameraManager;
	}

	const WorldCameraManager& World::GetCameraManager() const noexcept {
		return mCameraManager;
	}

	GameplayCueBus& World::GetGameplayCueBus() noexcept {
		return mGameplayCueBus;
	}

	const GameplayCueBus& World::GetGameplayCueBus() const noexcept {
		return mGameplayCueBus;
	}

	SequenceRuntime& World::GetSequenceRuntime() noexcept {
		if (!mSequenceRuntime) {
			mSequenceRuntime = std::make_unique<SequenceRuntime>(this);
		}
		return *mSequenceRuntime;
	}

	const SequenceRuntime& World::GetSequenceRuntime() const noexcept {
		return const_cast<World*>(this)->GetSequenceRuntime();
	}

	void World::SetScene(std::unique_ptr<Scene> scene) {
		mCameraManager.SetWorld(this);
		if (scene) {
			scene->SetWorld(this);
			for (const auto& entity : scene->GetEntities()) {
				if (!entity) {
					continue;
				}
				entity->SetScene(scene.get());
			}
		}
		mScene = std::move(scene);
		if (mScene) {
			++mSceneGeneration;
		}
		mCameraManager.ClearCurrentCamera();
		if (mSequenceRuntime) {
			mSequenceRuntime->Clear();
		}
	}

	Physics::Engine& World::GetPhysicsEngine() {
		return *mPhysicsEngine;
	}

	const Physics::Engine& World::GetPhysicsEngine() const {
		return *mPhysicsEngine;
	}

	void World::SetPostFxPassEnabledOverride(
		const std::string_view passName, const bool enabled
	) {
		if (passName.empty()) {
			return;
		}
		mPostFxPassOverrides[std::string(passName)].enabled = enabled;
	}

	void World::ClearPostFxPassEnabledOverride(
		const std::string_view passName
	) {
		if (passName.empty()) {
			return;
		}
		const auto it = mPostFxPassOverrides.find(std::string(passName));
		if (it == mPostFxPassOverrides.end()) {
			return;
		}
		it->second.enabled.reset();
		if (it->second.scalarParams.empty() && it->second.colorParams.empty()) {
			mPostFxPassOverrides.erase(it);
		}
	}

	void World::ClearPostFxPassEnabledOverrides() {
		for (auto it = mPostFxPassOverrides.begin();
		     it != mPostFxPassOverrides.end();) {
			it->second.enabled.reset();
			if (it->second.scalarParams.empty() && it->second.colorParams.
			    empty()) {
				it = mPostFxPassOverrides.erase(it);
			} else {
				++it;
			}
		}
	}

	void World::SetPostFxScalarOverride(
		const std::string_view passName, const std::string_view paramName,
		const float            value
	) {
		if (passName.empty() || paramName.empty()) {
			return;
		}
		mPostFxPassOverrides[std::string(passName)].scalarParams[std::string(
			paramName
		)] = value;
	}

	void World::SetPostFxColorOverride(
		const std::string_view passName, const std::string_view paramName,
		const Vec4&            value
	) {
		if (passName.empty() || paramName.empty()) {
			return;
		}
		mPostFxPassOverrides[std::string(passName)].colorParams[std::string(
			paramName
		)] = value;
	}

	void World::ClearPostFxParamOverride(
		const std::string_view passName, const std::string_view paramName
	) {
		if (passName.empty() || paramName.empty()) {
			return;
		}
		const auto it = mPostFxPassOverrides.find(std::string(passName));
		if (it == mPostFxPassOverrides.end()) {
			return;
		}
		it->second.scalarParams.erase(std::string(paramName));
		it->second.colorParams.erase(std::string(paramName));
		if (
			!it->second.enabled.has_value() &&
			it->second.scalarParams.empty() &&
			it->second.colorParams.empty()
		) {
			mPostFxPassOverrides.erase(it);
		}
	}

	void World::ClearPostFxPassOverrides(const std::string_view passName) {
		if (passName.empty()) {
			return;
		}
		mPostFxPassOverrides.erase(std::string(passName));
	}

	void World::ClearAllPostFxOverrides() {
		mPostFxPassOverrides.clear();
	}

	WorldDebugDraw& World::GetDebugDraw() noexcept {
		return mDebugDraw;
	}

	const WorldDebugDraw& World::GetDebugDraw() const noexcept {
		return mDebugDraw;
	}

	void World::QueueDebugScreenSprite(
		const Render::ScreenSpriteInput& sprite
	) {
		mDebugScreenSprites.emplace_back(sprite);
	}

	void World::QueueDebugScreenSprite(Render::ScreenSpriteInput&& sprite) {
		mDebugScreenSprites.emplace_back(std::move(sprite));
	}

	const WorldTime& World::GetTime() const noexcept {
		return mTime;
	}

	void World::SetServices(const WorldServices& services) noexcept {
		// ワールドごとに必要な外部サービスを保持し、
		// ServiceLocator への直接依存を減らします。
		mServices = services;
	}

	void World::SetSceneLoadOptions(
		const SceneLoadOptions& options
	) noexcept {
		mSceneLoadOptions = options;
	}

	const WorldServices& World::GetServices() const noexcept {
		return mServices;
	}

	ConsoleSystem* World::GetConsoleSystem() const noexcept {
		return mServices.console;
	}

	InputSystem* World::GetInputSystem() const noexcept {
		return mServices.inputSystem;
	}

	Profiler* World::GetProfiler() const noexcept {
		return mServices.profiler;
	}

	AssetManager* World::GetAssetManager() const noexcept {
		return mServices.assetManager;
	}

	IDemoService* World::GetDemoService() noexcept {
		return ServiceLocator::Get<IDemoService>(); // TODO: サービス乱用 ダメ
	}

	AudioSystem* World::GetAudioSystem() const noexcept {
		return mServices.audioSystem;
	}

	bool World::BuildUiInputCamera(Render::RenderCameraInput& outCamera) const {
		outCamera = {};
		return false;
	}

	void World::OnSceneLoaded() {
	}

	void World::OnSceneUnloaded() {
	}
}
