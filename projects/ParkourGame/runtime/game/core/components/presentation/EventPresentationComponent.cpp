#include "EventPresentationComponent.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <unordered_set>

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/assets/types/EventPresentationAssetData.h"
#include "core/content/ContentPathResolver.h"
#include "core/filesystem/Path.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/string/StrUtil.h"
#include "game/core/components/AudioFxControllerComponent.h"
#include "game/core/components/CameraFxControllerComponent.h"
#include "game/core/presentation/EventPresentationExecutor.h"
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
#include "game/core/presentation/editor/EventPresentationEditorGraphCodec.h"
#include "game/core/presentation/editor/EventPresentationEditorGraphUi.h"
#include "game/core/presentation/editor/EventPresentationEditorGraphValidator.h"
#endif

#include "engine/ImGui/Icons.h"
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
#include "engine/ImGui/ImGuiWidgets.h"
#endif
#include "engine/scene/Scene.h"
#include "engine/scene/SceneLoadOptions.h"
#include "engine/content/ContentMountDefinitions.h"
#include "engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/world/World.h"

namespace Unnamed {
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	struct EventPresentationGraphEditorState final {
		bool                                            windowOpen   = false;
		bool                                            dirty        = false;
		bool                                            needsRebuild = true;
		std::string                                     status;
		EventPresentationEditorGraph                    graph;
		std::vector<EventPresentationValidationIssue>   issues;
		std::unique_ptr<EventPresentationEditorGraphUi> ui;
	};
#endif

	namespace {
		constexpr std::string_view kChannel = "EventPresentationV2";

		/// @brief エンティティが指定 stableName のコンポーネントを持つか判定します。
		[[nodiscard]] bool HasComponentStableName(
			const Entity&          entity,
			const std::string_view stableName
		) {
			bool found = false;
			entity.ForEachComponent(
				[&found, stableName](const BaseComponent& component) {
					if (component.GetStableName() == stableName) {
						found = true;
						return false;
					}
					return true;
				}
			);
			return found;
		}
	}

	void EventPresentationComponent::OnAttached() {
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		if (!mGraphEditorState) {
			mGraphEditorState = std::make_unique<
				EventPresentationGraphEditorState>();
		}
		mGraphEditorState->needsRebuild = true;
#endif
		// コールバック登録前に実行対象とトリガーを揃え、即時発火でも不完全な状態を避ける
		mAudioFx   = ResolveAudioFx();
		mCameraFx  = ResolveCameraFx();
		mAnimation = ResolveAnimation();
		if (AssetManager* assetManager = GetAssetManager()) {
			(void)RebuildRuntimeData(*assetManager);
		}
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		AuditSourceGuidBindings();
#endif
		SubscribeAll();
	}

	void EventPresentationComponent::OnDetached() {
		UnsubscribeAll();
		mAudioFx   = nullptr;
		mCameraFx  = nullptr;
		mAnimation = nullptr;
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		if (mGraphEditorState) {
			mGraphEditorState->needsRebuild = true;
		}
#endif
	}

	void EventPresentationComponent::OnRenderTick(
		const float renderDeltaTime,
		const float interpolationAlpha
	) {
		(void)interpolationAlpha;
		mElapsedSeconds += std::max(0.0f, renderDeltaTime);
		if (!mAudioFx) {
			mAudioFx = ResolveAudioFx();
		}
		if (!mCameraFx) {
			mCameraFx = ResolveCameraFx();
		}
		if (!mAnimation) {
			mAnimation = ResolveAnimation();
		}
		RefreshAssetIfNeeded();
	}

	std::string_view EventPresentationComponent::GetStableName() const {
		return "game.EventPresentation";
	}

	std::string_view EventPresentationComponent::GetComponentName() const {
		return "EventPresentation";
	}

	uint32_t EventPresentationComponent::GetIcon() const {
		return kIconAccessibility;
	}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	void EventPresentationComponent::DrawInspectorImGui() {
		World*        world            = GetWorld();
		const Entity* owner            = GetOwner();
		bool          needsReload      = false;
		bool          needsResubscribe = false;
		std::string   assetPath        = !mEditorPresentationPath.IsEmpty() ?
			                                 mEditorPresentationPath.
			                                 ToGenericUtf8() :
			                                 mPresentationPath.has_value() ?
			                                 mPresentationPath->String() :
			                                 std::string{};
		const uint64_t ownerGuid = owner ? owner->GetGuid() : 0;

		ImGui::Text(
			"Owner GUID: %llu",
			static_cast<unsigned long long>(ownerGuid)
		);
		ImGui::Text(
			"Cue Source GUID (effective): %llu",
			static_cast<unsigned long long>(ResolveCueSourceEntityGuid())
		);
		ImGui::Text(
			"Audio Target GUID (effective): %llu",
			static_cast<unsigned long long>(ResolveAudioTargetEntityGuid())
		);
		ImGui::Text(
			"Camera Target GUID (effective): %llu",
			static_cast<unsigned long long>(ResolveCameraFxTargetEntityGuid())
		);
		ImGui::Text(
			"Animation Target GUID (effective): %llu",
			static_cast<unsigned long long>(ResolveAnimationTargetEntityGuid())
		);
		ImGui::Text("Asset Path: %s", assetPath.c_str());
		ImGui::Text(
			"Asset State: %s",
			mPresentationAssetId != kInvalidAssetID ? "Connected" : "Missing"
		);
		ImGui::Text("Trigger Count: %d", static_cast<int>(mTriggers.size()));
		ImGui::Text("Subscriptions: %d", static_cast<int>(mCueHandles.size()));

		if (
			ImGuiWidgets::AssetPathPicker(
				"Event Presentation Asset",
				assetPath,
				ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::EVENT_PRESENTATION)
			)
		) {
			AssetManager*                      assetManager = GetAssetManager();
			std::optional<ResolvedContentFile> selection;
			if (assetManager) {
				const auto virtualPath =
					VirtualPath::ParseContentReference(assetPath);
				if (virtualPath.has_value()) {
					selection = assetManager->GetContentPathResolver().
					                          ResolveFile(
						                          *virtualPath
					                          );
				} else {
					std::error_code ec;
					const Path      inputPath(assetPath);
					const Path      physicalPath = inputPath.IsAbsolute() ?
						                               inputPath.
						                               LexicallyNormal() :
						                               Path::FromNative(
							                               std::filesystem::absolute(
								                               inputPath.
								                               Native(), ec
							                               )).LexicallyNormal();
					const auto mountId = !ec ?
						                     assetManager->
						                     GetContentPathResolver().
						                     FindMountIdForResolvedPath(
							                     physicalPath) :
						                     std::nullopt;
					if (mountId.has_value()) {
						selection = assetManager->GetContentPathResolver().
						                          DescribePathFromMount(
							                          *mountId, physicalPath);
					}
				}
			}

			if (assetManager && selection.has_value()) {
				mEditorPresentationPath = selection->resolvedPath;
				(void)SetPresentationPath(
					selection->virtualPath, *assetManager
				);
			} else {
				mEditorPresentationPath.Clear();
				ClearPresentationPath();
			}
			needsResubscribe = true;
		}

		if (ImGui::InputScalar(
			"Cue Source Entity GUID", ImGuiDataType_U64, &mCueSourceEntityGuid
		)) {
			needsResubscribe = true;
		}
		if (ImGui::InputScalar(
			"Audio Target Entity GUID", ImGuiDataType_U64, &mAudioFxEntityGuid
		)) {
			mAudioFx = ResolveAudioFx();
		}
		if (ImGui::InputScalar(
				"Camera Target Entity GUID", ImGuiDataType_U64,
				&mCameraFxEntityGuid
			)
		) {
			mCameraFx = ResolveCameraFx();
		}
		if (ImGui::InputScalar(
			"Animation Target Entity GUID", ImGuiDataType_U64,
			&mAnimationEntityGuid
		)) {
			mAnimation = ResolveAnimation();
		}
		if (ImGui::Checkbox("Verbose Log", &mVerboseLog)) {
			needsReload = false;
		}

		if (ImGui::Button("Reload Asset")) {
			needsReload = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Rebind Subscriptions")) {
			needsResubscribe = true;
		}

		ImGui::SeparatorText("Debug");
		ImGui::Text(
			"Handled Cues: %llu",
			static_cast<unsigned long long>(mDebugHandledCueCount)
		);
		(void)ImGuiWidgets::InputText(
			"Publish Cue ID", mDebugPublishCueId, 256
		);
		(void)ImGui::DragFloat(
			"Publish Value", &mDebugPublishValue, 0.01f, -10000.0f, 10000.0f,
			"%.3f"
		);
		(void)ImGui::DragFloat(
			"Publish Value2",
			&mDebugPublishValue2,
			0.01f,
			-10000.0f,
			10000.0f,
			"%.3f"
		);
		if (ImGui::Button("Publish Test Cue")) {
			if (world && !mDebugPublishCueId.empty()) {
				GameplayCue cue      = {};
				cue.id               = mDebugPublishCueId;
				cue.sourceEntityGuid = ResolveCueSourceEntityGuid();
				cue.value            = mDebugPublishValue;
				cue.value2           = mDebugPublishValue2;
				if (cue.sourceEntityGuid != 0) {
					world->GetGameplayCueBus().Publish(cue);
				}
			}
		}

#ifdef UNNAMED_WITH_EDITOR
		if (!mGraphEditorState) {
			mGraphEditorState = std::make_unique<
				EventPresentationGraphEditorState>();
		}
		ImGui::SeparatorText("Event Graph Editor");
		if (ImGui::Button("Open Event Graph Editor")) {
			mGraphEditorState->windowOpen   = true;
			mGraphEditorState->needsRebuild = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Mark Graph Rebuild")) {
			mGraphEditorState->needsRebuild = true;
		}
		ImGui::Text(
			"Graph State: %s | きちゃない: %s",
			mGraphEditorState->windowOpen ? "Open" : "Closed",
			mGraphEditorState->dirty ? "Yes" : "No"
		);
		if (!mGraphEditorState->status.empty()) {
			ImGui::TextWrapped(
				"グラフの状態: %s", mGraphEditorState->status.c_str()
			);
		}
#endif

		if (needsReload) {
			AssetManager* assetManager = GetAssetManager();
			if (assetManager && mPresentationPath.has_value()) {
				mPresentationAssetId = assetManager->LoadAsset(
					*mPresentationPath,
					ASSET_TYPE::EVENT_PRESENTATION,
					AssetManager::AssetLoadPolicy::ForceReload
				);
				(void)RebuildRuntimeData(*assetManager);
			}
		}
		if (needsResubscribe) {
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
			AuditSourceGuidBindings();
#endif
			SubscribeAll();
		}

#ifdef UNNAMED_WITH_EDITOR
		DrawGraphEditorWindow();
#endif
	}

	void EventPresentationComponent::AuditSourceGuidBindings() const {
		const Entity* owner = GetOwner();
		if (!owner) {
			return;
		}

		const bool hasMovementPublisher =
			HasComponentStableName(*owner, "parkour.ParkourMovement") ||
			HasComponentStableName(*owner, "game.GameMovement") ||
			HasComponentStableName(*owner, "parkour.CourseProgress");
		const bool hasWeaponPublisher =
			HasComponentStableName(*owner, "game.WeaponSystem");
		const bool hasInventoryPublisher =
			HasComponentStableName(*owner, "game.InventorySystem");
		const bool hasPublisher = hasMovementPublisher || hasWeaponPublisher ||
		                          hasInventoryPublisher;
		if (!hasPublisher) {
			if (mCueSourceEntityGuid != 0 && mCueSourceEntityGuid != owner->
			    GetGuid()) {
				DevMsg(
					kChannel,
					"[SourceGuidAudit] owner has no known publishers; external cueSource configured: ownerGuid={} configured={} effective={}. Verify publisher owner GUID on runtime.",
					owner->GetGuid(),
					mCueSourceEntityGuid,
					ResolveCueSourceEntityGuid()
				);
			}
			return;
		}

		const uint64_t ownerGuid          = owner->GetGuid();
		const uint64_t configuredSource   = mCueSourceEntityGuid;
		const uint64_t effectiveSource    = ResolveCueSourceEntityGuid();
		const bool     sourceGuidMismatch = effectiveSource != ownerGuid;
		if (sourceGuidMismatch) {
			Warning(
				kChannel,
				"[SourceGuidAudit] cueSource mismatch: ownerGuid={} configured={} effective={} movement={} weapon={} inventory={}",
				ownerGuid,
				configuredSource,
				effectiveSource,
				hasMovementPublisher ? 1 : 0,
				hasWeaponPublisher ? 1 : 0,
				hasInventoryPublisher ? 1 : 0
			);
		} else if (mVerboseLog) {
			DevMsg(
				kChannel,
				"[SourceGuidAudit] source ok: ownerGuid={} configured={} effective={} movement={} weapon={} inventory={}",
				ownerGuid,
				configuredSource,
				effectiveSource,
				hasMovementPublisher ? 1 : 0,
				hasWeaponPublisher ? 1 : 0,
				hasInventoryPublisher ? 1 : 0
			);
		}
	}

#ifdef UNNAMED_WITH_EDITOR
	void EventPresentationComponent::DrawGraphEditorWindow() {
		if (!mGraphEditorState || !mGraphEditorState->windowOpen) {
			return;
		}

		bool             open             = mGraphEditorState->windowOpen;
		ImGuiWindowFlags graphWindowFlags = 0;
		// グラフ側が入力を保持している間は親ウィンドウ移動を抑止します。
		if (mGraphEditorState->ui && mGraphEditorState->ui->
		    IsCapturingMouseInput()) {
			graphWindowFlags |= ImGuiWindowFlags_NoMove;
		}
		if (
			!ImGui::Begin(
				"EventPresentation Graph Editor",
				&open,
				graphWindowFlags
			)
		) {
			ImGui::End();
			mGraphEditorState->windowOpen = open;
			return;
		}

		EventPresentationGraphEditorState& state = *mGraphEditorState;
		if (!state.ui) {
			state.ui = std::make_unique<EventPresentationEditorGraphUi>();
		}

		const auto ValidateGraph = [&state]() {
			(void)EventPresentationEditorGraphValidator::Validate(
				state.graph, state.issues
			);
		};

		if (state.needsRebuild) {
			state.graph.Clear();
			state.issues.clear();

			if (mEditorPresentationPath.IsEmpty()) {
				state.status =
					"グラフの再構築をスキップしました:アセットパスが空です。";
			} else if (AssetManager* assetManager = GetAssetManager()) {
				const AssetID assetId = assetManager->LoadAssetFromFile(
					mEditorPresentationPath,
					ASSET_TYPE::EVENT_PRESENTATION,
					AssetManager::AssetLoadPolicy::ForceReload
				);
				const EventPresentationAssetData* assetData =
					assetId != kInvalidAssetID ?
						assetManager->Get<EventPresentationAssetData>(assetId) :
						nullptr;
				if (!assetData) {
					state.status = "グラフの再構築に失敗:イベントプレゼンテーションアセットが欠落しています。";
				} else {
					std::string error;
					if (!EventPresentationEditorGraphCodec::BuildGraphFromAsset(
						*assetData,
						state.graph,
						mEditorPresentationPath.ToGenericUtf8(),
						&error
					)) {
						state.status = "グラフの再構築に失敗: " + error;
					} else {
						state.ui->ResetForGraph(state.graph);
						ValidateGraph();
						state.dirty  = false;
						state.status = "アセットからグラフを再構築しました。";
					}
				}
			} else {
				state.status =
					"グラフの再構築に失敗:AssetManagerが利用できません。";
			}
			state.needsRebuild = false;
		}

		if (ImGui::Button("アセットから再読み込み")) {
			state.needsRebuild = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Validate")) {
			ValidateGraph();
			state.status = "Validation completed.";
		}
		ImGui::SameLine();
		const bool canUndo = state.ui->CanUndo();
		if (!canUndo) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("取り消し")) {
			if (state.ui->Undo(state.graph)) {
				state.dirty = true;
				ValidateGraph();
				state.status = "元に戻しました。";
			}
		}
		if (!canUndo) {
			ImGui::EndDisabled();
		}
		ImGui::SameLine();
		const bool canRedo = state.ui->CanRedo();
		if (!canRedo) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("やり直し")) {
			if (state.ui->Redo(state.graph)) {
				state.dirty = true;
				ValidateGraph();
				state.status = "やり直しました。";
			}
		}
		if (!canRedo) {
			ImGui::EndDisabled();
		}
		bool snapEnabled = state.ui->IsGridSnapEnabled();
		if (ImGui::Checkbox("グリッドスナップ", &snapEnabled)) {
			state.ui->SetGridSnapEnabled(snapEnabled);
		}
		ImGui::SameLine();
		int snapSize = state.ui->GetGridSnapSize();
		if (ImGui::BeginCombo("スナップ", std::to_string(snapSize).c_str())) {
			constexpr int snapOptions[] = {16, 32, 64};
			for (const int option : snapOptions) {
				const bool selected = snapSize == option;
				if (ImGui::Selectable(
					std::to_string(option).c_str(), selected
				)) {
					state.ui->SetGridSnapSize(option);
					snapSize = option;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::TextDisabled("Shift/Altキーを押し続けてスナップ解除");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("ノードをドラッグ時の一時的なスナップ解除。");
		}
		ImGui::SameLine();
		if (ImGui::Button("グラフを保存")) {
			ValidateGraph();
			const bool hasError = std::ranges::any_of(
				state.issues,
				[](const EventPresentationValidationIssue& issue) {
					return issue.severity ==
					       EventPresentationValidationSeverity::Error;
				}
			);
			if (hasError) {
				state.status = "セーブ中止:グラフにエラーがあります。";
			} else if (AssetManager* assetManager = GetAssetManager();
				!assetManager) {
				state.status =
					"セーブ中止:AssetManagerが利用できません。";
			} else if (assetManager->GetContentPathResolver().
			                         FindMountIdForResolvedPath(
				                         mEditorPresentationPath) ==
			           ContentMountId::kCore) {
				state.status =
					"セーブ中止:Coreのグラフは読み取り専用です。Game contentへ複製してください。";
			} else {
				EventPresentationAssetData assetData = {};
				std::string                error;
				if (!EventPresentationEditorGraphCodec::BuildAssetFromGraph(
					state.graph,
					assetData,
					&error
				)) {
					state.status = "セーブ失敗(グラフ->アセット): " + error;
				} else if (!EventPresentationEditorGraphCodec::SaveAssetJson(
					assetData,
					&state.graph,
					mEditorPresentationPath.ToGenericUtf8(),
					&error
				)) {
					state.status = "セーブ失敗(書き込み): " + error;
				} else {
					state.status = "グラフはアセットJSONに保存されます。";
					state.dirty  = false;
					if (assetManager) {
						const AssetID reloadedId =
							assetManager->LoadAssetFromFile(
								mEditorPresentationPath,
								ASSET_TYPE::EVENT_PRESENTATION,
								AssetManager::AssetLoadPolicy::ForceReload
							);
						if (reloadedId == mPresentationAssetId) {
							(void)RebuildRuntimeData(*assetManager);
						}
					}
					SubscribeAll();
					state.needsRebuild = true;
				}
			}
		}

		const std::string assetPathText =
			mEditorPresentationPath.ToGenericUtf8();
		ImGui::Text("Asset Path: %s", assetPathText.c_str());
		ImGui::Text(
			"Selected Node: %llu | きちゃない: %s",
			static_cast<unsigned long long>(state.ui->GetSelectedNodeId()),
			state.dirty ? "Yes" : "No"
		);
		if (!state.status.empty()) {
			ImGui::TextWrapped("Status: %s", state.status.c_str());
		}
		ImGui::Separator();

		if (state.ui->Draw(state.graph, state.issues)) {
			state.dirty = true;
			ValidateGraph();
		}

		ImGui::End();
		mGraphEditorState->windowOpen = open;
	}
#endif
#endif

	void EventPresentationComponent::Deserialize(const JsonReader& reader) {
		constexpr SceneLoadOptions options{};
		const Path scenePath("<direct component deserialize>");
		const SceneDeserializeContext context{
			.loadOptions   = options,
			.assetManager  = GetAssetManager(),
			.scenePath     = scenePath,
			.entityName    = "<unknown>",
			.entityId      = 0,
			.componentType = GetStableName(),
		};
		(void)Deserialize(reader, context);
	}

	bool EventPresentationComponent::Deserialize(
		const JsonReader& reader, const SceneDeserializeContext& context
	) {
		mCueSourceEntityGuid = 0;
		mAudioFxEntityGuid   = 0;
		mCameraFxEntityGuid  = 0;
		mAnimationEntityGuid = 0;
		mVerboseLog          = false;
		UnsubscribeAll();
		ClearPresentationPath();
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		mEditorPresentationPath.Clear();
#endif

		if (reader.Has("cueSourceEntityGuid")) {
			mCueSourceEntityGuid = reader["cueSourceEntityGuid"].GetUint64();
		}
		if (reader.Has("audioFxEntityGuid")) {
			mAudioFxEntityGuid = reader["audioFxEntityGuid"].GetUint64();
		}
		if (reader.Has("cameraFxEntityGuid")) {
			mCameraFxEntityGuid = reader["cameraFxEntityGuid"].GetUint64();
		}
		if (reader.Has("animationEntityGuid")) {
			mAnimationEntityGuid = reader["animationEntityGuid"].GetUint64();
		}
		if (reader.Has("verboseLog")) {
			mVerboseLog = reader["verboseLog"].GetBool(false);
		}

		const JsonReader pathNode = reader["assetPath"];
		if (!pathNode.Valid()) {
			return true;
		}
		if (!pathNode.IsString()) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='invalid virtual path' scene='{}' entity='{}' entityId={} component='{}' field='assetPath' reason='expected string'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType
			);
			return !IsStrictAssetValidation(context.loadOptions);
		}

		const std::string pathValue = pathNode.GetString();
		if (pathValue.empty()) {
			return true;
		}
		const std::optional<VirtualPath> virtualPath =
			VirtualPath::ParseContentReference(pathValue);
		if (!virtualPath.has_value()) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='invalid virtual path' scene='{}' entity='{}' entityId={} component='{}' field='assetPath' virtualPath='{}'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType,
				pathValue
			);
			return !IsStrictAssetValidation(context.loadOptions);
		}
		if (!context.assetManager) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='assetPath' virtualPath='{}' reason='AssetManager unavailable'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType,
				pathValue
			);
			return !IsStrictAssetValidation(context.loadOptions);
		}

		const std::optional<ResolvedContentFile> resolvedFile =
			context.assetManager->GetContentPathResolver().ResolveFile(
				*virtualPath);
		if (!resolvedFile.has_value()) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='unresolved content' scene='{}' entity='{}' entityId={} component='{}' field='assetPath' virtualPath='{}'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType,
				pathValue
			);
			return !IsStrictAssetValidation(context.loadOptions);
		}
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		mEditorPresentationPath = resolvedFile->resolvedPath;
#endif
		if (!SetPresentationPath(*virtualPath, *context.assetManager)) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='assetPath' virtualPath='{}' mount='{}' physicalPath='{}'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType,
				pathValue,
				resolvedFile->mountId,
				resolvedFile->resolvedPath
			);
			ClearPresentationPath();
			return !IsStrictAssetValidation(context.loadOptions);
		}

		mAudioFx   = ResolveAudioFx();
		mCameraFx  = ResolveCameraFx();
		mAnimation = ResolveAnimation();
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		AuditSourceGuidBindings();
#endif
		SubscribeAll();
		return true;
	}

	void EventPresentationComponent::Serialize(JsonWriter& writer) const {
		if (mPresentationPath.has_value()) {
			writer.Key("assetPath");
			writer.Write(mPresentationPath->String());
		}
		writer.Key("cueSourceEntityGuid");
		writer.Write(mCueSourceEntityGuid);
		writer.Key("audioFxEntityGuid");
		writer.Write(mAudioFxEntityGuid);
		writer.Key("cameraFxEntityGuid");
		writer.Write(mCameraFxEntityGuid);
		writer.Key("animationEntityGuid");
		writer.Write(mAnimationEntityGuid);
		writer.Key("verboseLog");
		writer.Write(mVerboseLog);
	}

	bool EventPresentationComponent::SetPresentationPath(
		const VirtualPath& path, AssetManager& assetManager
	) {
		const AssetID assetId = assetManager.LoadAsset(
			path, ASSET_TYPE::EVENT_PRESENTATION
		);
		if (assetId == kInvalidAssetID) {
			ClearPresentationPath();
			return false;
		}
		mPresentationPath    = path;
		mPresentationAssetId = assetId;
		mLoadedAssetVersion  = 0;
		mLoadedAssetName.clear();
		mTriggers.clear();
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		if (mGraphEditorState) {
			mGraphEditorState->needsRebuild = true;
		}
#endif
		if (RebuildRuntimeData(assetManager)) {
			return true;
		}
		ClearPresentationPath();
		return false;
	}

	void EventPresentationComponent::ClearPresentationPath() {
		mPresentationPath.reset();
		mPresentationAssetId = kInvalidAssetID;
		mLoadedAssetVersion  = 0;
		mLoadedAssetName.clear();
		mTriggers.clear();
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		if (mGraphEditorState) {
			mGraphEditorState->needsRebuild = true;
		}
#endif
	}

	bool EventPresentationComponent::RebuildRuntimeData(
		AssetManager& assetManager
	) {
		mTriggers.clear();
		mLoadedAssetVersion = 0;
		mLoadedAssetName.clear();

		if (
			!mPresentationPath.has_value() ||
			mPresentationAssetId == kInvalidAssetID
		) {
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
			if (mGraphEditorState) {
				mGraphEditorState->needsRebuild = true;
			}
#endif
			return false;
		}

		const auto* assetData = assetManager.Get<EventPresentationAssetData>(
			mPresentationAssetId
		);
		if (!assetData) {
			Warning(
				kChannel,
				"Asset '{}' is not EventPresentationAssetData.",
				mPresentationPath->String()
			);
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
			if (mGraphEditorState) {
				mGraphEditorState->needsRebuild = true;
			}
#endif
			return false;
		}

		mLoadedAssetVersion = assetManager.Meta(mPresentationAssetId).version;
		mLoadedAssetName    = assetData->name;
		mTriggers.reserve(assetData->triggers.size());

		for (const EventPresentationTriggerAssetData& triggerData : assetData->
		     triggers) {
			EventPresentationTrigger trigger = {};
			trigger.cueId = StrUtil::TrimAsciiWhitespace(triggerData.cueId);
			if (trigger.cueId.empty()) {
				continue;
			}
			trigger.cooldownSec       = std::max(0.0f, triggerData.cooldownSec);
			trigger.condition.enabled = triggerData.condition.enabled;
			trigger.condition.source  =
				EventPresentationExecutor::ParseValueSource(
					triggerData.condition.source,
					&trigger.condition.payloadName
				);
			trigger.condition.minValue = triggerData.condition.minValue;
			trigger.condition.maxValue = triggerData.condition.maxValue;
			if (trigger.condition.maxValue < trigger.condition.minValue) {
				std::swap(
					trigger.condition.minValue, trigger.condition.maxValue
				);
			}

			for (const EventPresentationActionAssetData& actionData :
			     triggerData.
			     actions) {
				EventPresentationAction action = {};
				action.typeName = StrUtil::TrimAsciiWhitespace(actionData.type);
				action.actionType = EventPresentationExecutor::ParseActionType(
					action.typeName
				);
				action.id = StrUtil::TrimAsciiWhitespace(actionData.id);
				action.debugText = actionData.debugText;
				action.value.source =
					EventPresentationExecutor::ParseValueSource(
						actionData.valueInput.source,
						&action.value.payloadName
					);
				action.value.constant     = actionData.valueInput.constant;
				action.value.clampEnabled = actionData.valueInput.clampEnabled;
				action.value.clampMin     = actionData.valueInput.clampMin;
				action.value.clampMax     = actionData.valueInput.clampMax;
				if (action.value.clampMax < action.value.clampMin) {
					std::swap(action.value.clampMin, action.value.clampMax);
				}
				action.value.multiply = actionData.valueInput.multiply;
				trigger.actions.emplace_back(std::move(action));
			}

			if (!trigger.actions.empty()) {
				mTriggers.emplace_back(std::move(trigger));
			}
		}

		if (mVerboseLog) {
			DevMsg(
				kChannel,
				"Loaded v2 asset '{}' with {} triggers.",
				mPresentationPath->String(),
				static_cast<int>(mTriggers.size())
			);
		}
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		if (mGraphEditorState) {
			mGraphEditorState->needsRebuild = true;
		}
#endif
		return true;
	}

	void EventPresentationComponent::RefreshAssetIfNeeded() {
		if (!mPresentationPath.has_value()) {
			if (mPresentationAssetId != kInvalidAssetID || !mTriggers.empty()) {
				mPresentationAssetId = kInvalidAssetID;
				mLoadedAssetVersion  = 0;
				mLoadedAssetName.clear();
				mTriggers.clear();
				SubscribeAll();
			}
			return;
		}

		AssetManager* assetManager = GetAssetManager();
		if (!assetManager) {
			return;
		}

		if (mPresentationAssetId == kInvalidAssetID) {
			return;
		}
		const auto& meta = assetManager->Meta(mPresentationAssetId);
		if (meta.version != mLoadedAssetVersion) {
			// ホットリロードでトリガー集合が変わるため、購読も作り直す
			(void)RebuildRuntimeData(*assetManager);
			SubscribeAll();
		}
	}

	void EventPresentationComponent::SubscribeAll() {
		UnsubscribeAll();

		World* world = GetWorld();
		if (!world) {
			return;
		}

		const uint64_t sourceEntityGuid = ResolveCueSourceEntityGuid();
		if (sourceEntityGuid == 0) {
			Warning(
				kChannel,
				"SubscribeAll skipped: cue source guid is zero (owner missing)."
			);
			return;
		}

		// 同じ Cue ID の複数トリガーは一つのバス購読で受け取る
		std::unordered_set<std::string> uniqueCueIds;
		uniqueCueIds.reserve(mTriggers.size());
		for (const EventPresentationTrigger& trigger : mTriggers) {
			if (!trigger.cueId.empty()) {
				uniqueCueIds.emplace(trigger.cueId);
			}
		}

		mCueHandles.reserve(uniqueCueIds.size());
		for (const std::string& cueId : uniqueCueIds) {
			GameplayCueFilter filter            = {};
			filter.cueId                        = cueId;
			filter.sourceEntityGuid             = sourceEntityGuid;
			const GameplayCueBus::Handle handle = world->GetGameplayCueBus().
				Subscribe(
					filter,
					[this](const GameplayCue& cue) {
						HandleCue(cue);
					}
				);
			if (handle != 0) {
				mCueHandles.emplace_back(handle);
			}
		}
	}

	void EventPresentationComponent::UnsubscribeAll() {
		if (World* world = GetWorld()) {
			for (const GameplayCueBus::Handle handle : mCueHandles) {
				(void)world->GetGameplayCueBus().Unsubscribe(handle);
			}
		}
		mCueHandles.clear();
	}

	void EventPresentationComponent::HandleCue(const GameplayCue& cue) {
#ifdef _DEBUG
		++mDebugHandledCueCount;
#endif
		if (!mAudioFx) {
			mAudioFx = ResolveAudioFx();
		}
		if (!mCameraFx) {
			mCameraFx = ResolveCameraFx();
		}
		if (!mAnimation) {
			mAnimation = ResolveAnimation();
		}

		if (mVerboseLog) {
			DevMsg(
				kChannel,
				"Cue received id='{}' sourceGuid={} receiverGuid={} value={:.3f} value2={:.3f}.",
				cue.id,
				cue.sourceEntityGuid,
				GetOwner() ? GetOwner()->GetGuid() : 0,
				cue.value,
				cue.value2
			);
		}

		// 同じ Cue に一致する各トリガーは、条件とクールダウンを独立して評価する
		for (EventPresentationTrigger& trigger : mTriggers) {
			if (trigger.cueId != cue.id) {
				continue;
			}
			if (!EventPresentationExecutor::EvaluateCondition(
				trigger.condition, cue
			)) {
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
				if (mGraphEditorState && mGraphEditorState->ui) {
					mGraphEditorState->ui->NotifyTriggerConditionFailed(
						mGraphEditorState->graph, trigger.cueId
					);
				}
#endif
				if (mVerboseLog) {
					DevMsg(
						kChannel,
						"Trigger skipped by condition cue='{}'.",
						trigger.cueId
					);
				}
				continue;
			}
			if (
				trigger.cooldownSec > 0.0f &&
				(mElapsedSeconds - trigger.lastTriggerAt) < trigger.cooldownSec
			) {
				if (mVerboseLog) {
					DevMsg(
						kChannel,
						"Trigger skipped by cooldown cue='{}' cooldownSec={:.3f}.",
						trigger.cueId,
						trigger.cooldownSec
					);
				}
				continue;
			}
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
			if (mGraphEditorState && mGraphEditorState->ui) {
				mGraphEditorState->ui->NotifyTriggerExecuted(
					mGraphEditorState->graph, trigger.cueId
				);
			}
#endif

			const uint64_t receiverGuid =
				GetOwner() ? GetOwner()->GetGuid() : 0;
			const std::string assetDisplayName = mLoadedAssetName.empty() ?
				                                     (mPresentationPath.
					                                     has_value() ?
						                                     mPresentationPath->
						                                     String() :
						                                     std::string{}) :
				                                     mLoadedAssetName;
			const EventPresentationExecutor::ExecutionContext context = {
				.cue                       = cue,
				.assetName                 = std::string_view(assetDisplayName),
				.receiverEntityGuid        = receiverGuid,
				.verboseLog                = mVerboseLog,
				.audioFx                   = mAudioFx,
				.cameraFx                  = mCameraFx,
				.animation                 = mAnimation,
				.audioTargetEntityGuid     = ResolveAudioTargetEntityGuid(),
				.cameraTargetEntityGuid    = ResolveCameraFxTargetEntityGuid(),
				.animationTargetEntityGuid = ResolveAnimationTargetEntityGuid(),
#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
				.actionTraceCallback =
				[this, triggerCueId = std::string(trigger.cueId)](
				const size_t                                       actionIndex,
				const EventPresentationExecutor::ActionTraceStatus status
			) {
					if (!mGraphEditorState || !mGraphEditorState->ui) {
						return;
					}
					auto uiState =
						EventPresentationEditorGraphUi::RuntimeTraceState::Executed;
					switch (status) {
						case
						EventPresentationExecutor::ActionTraceStatus::Executed
						: uiState =
						  EventPresentationEditorGraphUi::RuntimeTraceState::Executed;
							break;
						case
						EventPresentationExecutor::ActionTraceStatus::Skipped
						: uiState =
						  EventPresentationEditorGraphUi::RuntimeTraceState::Skipped;
							break;
						case
						EventPresentationExecutor::ActionTraceStatus::Warning
						: uiState =
						  EventPresentationEditorGraphUi::RuntimeTraceState::Warning;
							break;
						case EventPresentationExecutor::ActionTraceStatus::Error
						: uiState =
						  EventPresentationEditorGraphUi::RuntimeTraceState::Error;
							break;
						default: break;
					}
					mGraphEditorState->ui->NotifyActionTrace(
						mGraphEditorState->graph,
						triggerCueId,
						actionIndex,
						uiState
					);
				},
#endif
			};

			if (mVerboseLog) {
				DevMsg(
					kChannel,
					"Trigger matched cue='{}' actionCount={} asset='{}'.",
					trigger.cueId,
					static_cast<int>(trigger.actions.size()),
					context.assetName
				);
			}
			EventPresentationExecutor::ExecuteActions(trigger, context);
			trigger.lastTriggerAt = mElapsedSeconds;
		}
	}

	uint64_t EventPresentationComponent::ResolveCueSourceEntityGuid() const {
		if (mCueSourceEntityGuid != 0) {
			return mCueSourceEntityGuid;
		}
		const Entity* owner = GetOwner();
		return owner ? owner->GetGuid() : 0;
	}

	uint64_t EventPresentationComponent::ResolveAudioTargetEntityGuid() const {
		// 明示 GUID があれば最優先。0 は「Owner を使う」予約値として扱う。
		if (mAudioFxEntityGuid != 0) {
			return mAudioFxEntityGuid;
		}
		const Entity* owner = GetOwner();
		return owner ? owner->GetGuid() : 0;
	}

	uint64_t
	EventPresentationComponent::ResolveCameraFxTargetEntityGuid() const {
		// 明示 GUID があれば最優先。0 は「Owner を使う」予約値として扱う。
		if (mCameraFxEntityGuid != 0) {
			return mCameraFxEntityGuid;
		}
		const Entity* owner = GetOwner();
		return owner ? owner->GetGuid() : 0;
	}

	uint64_t
	EventPresentationComponent::ResolveAnimationTargetEntityGuid() const {
		// 明示 GUID があれば最優先。0 は「Owner を使う」予約値として扱う。
		if (mAnimationEntityGuid != 0) {
			return mAnimationEntityGuid;
		}
		const Entity* owner = GetOwner();
		return owner ? owner->GetGuid() : 0;
	}

	AudioFxControllerComponent*
	EventPresentationComponent::ResolveAudioFx() const {
		const uint64_t targetGuid = ResolveAudioTargetEntityGuid();
		if (targetGuid == 0) {
			return nullptr;
		}
		World* world = GetWorld();
		Scene* scene = world ? world->GetScenePtr() : nullptr;
		if (!scene) {
			return nullptr;
		}
		Entity* target = scene->FindEntity(targetGuid);
		return target ?
			       target->GetComponent<AudioFxControllerComponent>() :
			       nullptr;
	}

	CameraFxControllerComponent*
	EventPresentationComponent::ResolveCameraFx() const {
		const uint64_t targetGuid = ResolveCameraFxTargetEntityGuid();
		if (targetGuid == 0) {
			return nullptr;
		}
		World* world = GetWorld();
		Scene* scene = world ? world->GetScenePtr() : nullptr;
		if (!scene) {
			return nullptr;
		}
		Entity* target = scene->FindEntity(targetGuid);
		return target ?
			       target->GetComponent<CameraFxControllerComponent>() :
			       nullptr;
	}

	SkeletalAnimationComponent*
	EventPresentationComponent::ResolveAnimation() const {
		const uint64_t targetGuid = ResolveAnimationTargetEntityGuid();
		if (targetGuid == 0) {
			return nullptr;
		}
		World* world = GetWorld();
		Scene* scene = world ? world->GetScenePtr() : nullptr;
		if (!scene) {
			return nullptr;
		}
		Entity* target = scene->FindEntity(targetGuid);
		return target ?
			       target->GetComponent<SkeletalAnimationComponent>() :
			       nullptr;
	}

	void RegisterEventPresentationComponent(
		ComponentRegistry& componentRegistry
	) {
		static constexpr std::string_view kStableName =
			"game.EventPresentation";
		static constexpr std::string_view kDisplayName = "EventPresentation";
		if (componentRegistry.IsRegistered(kStableName)) {
			return;
		}

		const bool registered = componentRegistry.Register(
			kStableName,
			[]() -> std::unique_ptr<BaseComponent> {
				return std::make_unique<EventPresentationComponent>();
			},
			kDisplayName
		);
		if (!registered) {
			Warning(
				"ParkourRuntime",
				"Failed to register game component '{}'.",
				kStableName
			);
		}
	}

	REGISTER_COMPONENT(EventPresentationComponent);
}
