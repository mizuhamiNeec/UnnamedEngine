#include "EventPresentationComponent.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <unordered_set>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/assets/types/EventPresentationAssetData.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/string/StrUtil.h"
#include "game/core/components/AudioFxControllerComponent.h"
#include "game/core/components/CameraFxControllerComponent.h"
#include "game/core/presentation/EventPresentationExecutor.h"
#ifdef _DEBUG
#include "game/core/presentation/editor/EventPresentationEditorGraphCodec.h"
#include "game/core/presentation/editor/EventPresentationEditorGraphUi.h"
#include "game/core/presentation/editor/EventPresentationEditorGraphValidator.h"
#endif

#include "engine/ImGui/Icons.h"
#ifdef _DEBUG
#include "engine/ImGui/ImGuiWidgets.h"
#endif
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/world/World.h"

namespace Unnamed {
#ifdef _DEBUG
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

		[[nodiscard]] std::string TrimAscii(std::string_view text) {
			size_t begin = 0;
			while (
				begin < text.size() &&
				std::isspace(static_cast<unsigned char>(text[begin])) != 0
			) {
				++begin;
			}
			size_t end = text.size();
			while (
				end > begin &&
				std::isspace(static_cast<unsigned char>(text[end - 1])) != 0
			) {
				--end;
			}
			return std::string(text.substr(begin, end - begin));
		}

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

#ifdef _DEBUG
		bool EditStringField(
			const char* label, std::string& value, const size_t capacity = 256
		) {
			std::vector<char> buffer(capacity, '\0');
			const size_t      copyLength = std::min(value.size(), capacity - 1);
			if (copyLength > 0) {
				std::memcpy(buffer.data(), value.data(), copyLength);
			}
			if (!ImGui::InputText(label, buffer.data(), buffer.size())) {
				return false;
			}
			value = buffer.data();
			return true;
		}

		bool EditEntityGuidField(const char* label, uint64_t& guid) {
			return ImGui::InputScalar(label, ImGuiDataType_U64, &guid);
		}
#endif
	}

	void EventPresentationComponent::OnAttached() {
#ifdef _DEBUG
		if (!mGraphEditorState) {
			mGraphEditorState = std::make_unique<
				EventPresentationGraphEditorState>();
		}
		mGraphEditorState->needsRebuild = true;
#endif
		mAudioFx   = ResolveAudioFx();
		mCameraFx  = ResolveCameraFx();
		mAnimation = ResolveAnimation();
		(void)LoadAsset();
#ifdef _DEBUG
		AuditSourceGuidBindings();
#endif
		SubscribeAll();
	}

	void EventPresentationComponent::OnDetached() {
		UnsubscribeAll();
		mAudioFx   = nullptr;
		mCameraFx  = nullptr;
		mAnimation = nullptr;
#ifdef _DEBUG
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

#ifdef _DEBUG
	void EventPresentationComponent::DrawInspectorImGui() {
		World*         world            = GetWorld();
		Entity*        owner            = GetOwner();
		bool           needsReload      = false;
		bool           needsResubscribe = false;
		std::string    assetPath        = mAssetPath;
		const uint64_t ownerGuid        = owner ? owner->GetGuid() : 0;

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
		ImGui::Text("Asset Path: %s", mAssetPath.c_str());
		ImGui::Text(
			"Asset State: %s",
			mAssetId != kInvalidAssetID ? "Connected" : "Missing"
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
			SetAssetPath(assetPath);
			needsReload      = true;
			needsResubscribe = true;
		}

		if (EditEntityGuidField(
			"Cue Source Entity GUID", mCueSourceEntityGuid
		)) {
			needsResubscribe = true;
		}
		if (EditEntityGuidField(
			"Audio Target Entity GUID", mAudioFxEntityGuid
		)) {
			mAudioFx = ResolveAudioFx();
		}
		if (EditEntityGuidField(
				"Camera Target Entity GUID", mCameraFxEntityGuid
			)
		) {
			mCameraFx = ResolveCameraFx();
		}
		if (EditEntityGuidField(
			"Animation Target Entity GUID", mAnimationEntityGuid
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
		(void)EditStringField("Publish Cue ID", mDebugPublishCueId);
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

		if (needsReload) {
			(void)LoadAsset();
		}
		if (needsResubscribe) {
#ifdef _DEBUG
			AuditSourceGuidBindings();
#endif
			SubscribeAll();
		}

		DrawGraphEditorWindow();
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

		const auto validateGraph = [&state]() {
			(void)EventPresentationEditorGraphValidator::Validate(
				state.graph, state.issues
			);
		};

		if (state.needsRebuild) {
			state.graph.Clear();
			state.issues.clear();

			if (mAssetPath.empty()) {
				state.status =
					"グラフの再構築をスキップしました:アセットパスが空です。";
			} else if (AssetManager* assetManager = GetAssetManager()) {
				AssetID assetId = mAssetId;
				if (assetId == kInvalidAssetID) {
					assetId = assetManager->LoadFromFile(
						mAssetPath, ASSET_TYPE::EVENT_PRESENTATION
					);
				}
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
						mAssetPath,
						&error
					)) {
						state.status = "グラフの再構築に失敗: " + error;
					} else {
						state.ui->ResetForGraph(state.graph);
						validateGraph();
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
			validateGraph();
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
				validateGraph();
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
				validateGraph();
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
			constexpr int kSnapOptions[] = {16, 32, 64};
			for (const int option : kSnapOptions) {
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
			validateGraph();
			const bool hasError = std::ranges::any_of(
				state.issues,
				[](const EventPresentationValidationIssue& issue) {
					return issue.severity ==
					       EventPresentationValidationSeverity::Error;
				}
			);
			if (hasError) {
				state.status = "セーブ中止:グラフにエラーがあります。";
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
					mAssetPath,
					&error
				)) {
					state.status = "セーブ失敗(書き込み): " + error;
				} else {
					state.status = "グラフはアセットJSONに保存されます。";
					state.dirty  = false;
					(void)LoadAsset();
					SubscribeAll();
					state.needsRebuild = true;
				}
			}
		}

		ImGui::Text("Asset Path: %s", mAssetPath.c_str());
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
			validateGraph();
		}

		ImGui::End();
		mGraphEditorState->windowOpen = open;
	}
#endif

	void EventPresentationComponent::Deserialize(const JsonReader& reader) {
		mCueSourceEntityGuid = 0;
		mAudioFxEntityGuid   = 0;
		mCameraFxEntityGuid  = 0;
		mAnimationEntityGuid = 0;
		mVerboseLog          = false;
		SetAssetPath("");

		if (reader.Has("assetPath")) {
			SetAssetPath(reader["assetPath"].GetString(""));
		}
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

		mAudioFx   = ResolveAudioFx();
		mCameraFx  = ResolveCameraFx();
		mAnimation = ResolveAnimation();
		(void)LoadAsset();
#ifdef _DEBUG
		AuditSourceGuidBindings();
#endif
		SubscribeAll();
	}

	void EventPresentationComponent::Serialize(JsonWriter& writer) const {
		writer.Key("assetPath");
		writer.Write(mAssetPath);
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

	void EventPresentationComponent::SetAssetPath(const std::string& path) {
		const std::string normalized = path.empty() ?
			                               std::string() :
			                               StrUtil::NormalizePath(path);
		if (mAssetPath == normalized) {
			return;
		}
		mAssetPath          = normalized;
		mAssetId            = kInvalidAssetID;
		mLoadedAssetVersion = 0;
		mLoadedAssetName.clear();
		mTriggers.clear();
#ifdef _DEBUG
		if (mGraphEditorState) {
			mGraphEditorState->needsRebuild = true;
		}
#endif
	}

	bool EventPresentationComponent::LoadAsset() {
		mTriggers.clear();
		mAssetId            = kInvalidAssetID;
		mLoadedAssetVersion = 0;
		mLoadedAssetName.clear();

		if (mAssetPath.empty()) {
#ifdef _DEBUG
			if (mGraphEditorState) {
				mGraphEditorState->needsRebuild = true;
			}
#endif
			return false;
		}

		AssetManager* assetManager = GetAssetManager();
		if (!assetManager) {
			Warning(kChannel, "AssetManager is not available.");
			return false;
		}

		const AssetID assetId = assetManager->LoadFromFile(
			mAssetPath, ASSET_TYPE::EVENT_PRESENTATION
		);
		if (assetId == kInvalidAssetID) {
			Warning(
				kChannel, "Failed to load event presentation '{}'.", mAssetPath
			);
#ifdef _DEBUG
			if (mGraphEditorState) {
				mGraphEditorState->needsRebuild = true;
			}
#endif
			return false;
		}

		const auto* assetData = assetManager->Get<EventPresentationAssetData>(
			assetId
		);
		if (!assetData) {
			Warning(
				kChannel,
				"Asset '{}' is not EventPresentationAssetData.",
				mAssetPath
			);
#ifdef _DEBUG
			if (mGraphEditorState) {
				mGraphEditorState->needsRebuild = true;
			}
#endif
			return false;
		}

		mAssetId            = assetId;
		mLoadedAssetVersion = assetManager->Meta(assetId).version;
		mLoadedAssetName    = assetData->name;
		mTriggers.reserve(assetData->triggers.size());

		for (const EventPresentationTriggerAssetData& triggerData : assetData->
		     triggers) {
			EventPresentationTrigger trigger = {};
			trigger.cueId                    = TrimAscii(triggerData.cueId);
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
				action.typeName = TrimAscii(actionData.type);
				action.actionType = EventPresentationExecutor::ParseActionType(
					action.typeName
				);
				action.id           = TrimAscii(actionData.id);
				action.debugText    = actionData.debugText;
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
				mAssetPath,
				static_cast<int>(mTriggers.size())
			);
		}
#ifdef _DEBUG
		if (mGraphEditorState) {
			mGraphEditorState->needsRebuild = true;
		}
#endif
		return true;
	}

	void EventPresentationComponent::RefreshAssetIfNeeded() {
		if (mAssetPath.empty()) {
			if (mAssetId != kInvalidAssetID || !mTriggers.empty()) {
				mAssetId            = kInvalidAssetID;
				mLoadedAssetVersion = 0;
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

		bool needsReload = false;
		if (mAssetId == kInvalidAssetID) {
			needsReload = true;
		} else {
			const auto& meta = assetManager->Meta(mAssetId);
			needsReload = !meta.loaded || meta.version != mLoadedAssetVersion;
		}

		if (needsReload) {
			(void)LoadAsset();
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

		for (EventPresentationTrigger& trigger : mTriggers) {
			if (trigger.cueId != cue.id) {
				continue;
			}
			if (!EventPresentationExecutor::EvaluateCondition(
				trigger.condition, cue
			)) {
#ifdef _DEBUG
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
#ifdef _DEBUG
			if (mGraphEditorState && mGraphEditorState->ui) {
				mGraphEditorState->ui->NotifyTriggerExecuted(
					mGraphEditorState->graph, trigger.cueId
				);
			}
#endif

			const uint64_t receiverGuid =
				GetOwner() ? GetOwner()->GetGuid() : 0;
			const EventPresentationExecutor::ExecutionContext context = {
				.cue       = cue,
				.assetName = mLoadedAssetName.empty() ?
					             std::string_view(mAssetPath) :
					             std::string_view(mLoadedAssetName),
				.receiverEntityGuid        = receiverGuid,
				.verboseLog                = mVerboseLog,
				.audioFx                   = mAudioFx,
				.cameraFx                  = mCameraFx,
				.animation                 = mAnimation,
				.audioTargetEntityGuid     = ResolveAudioTargetEntityGuid(),
				.cameraTargetEntityGuid    = ResolveCameraFxTargetEntityGuid(),
				.animationTargetEntityGuid = ResolveAnimationTargetEntityGuid(),
#ifdef _DEBUG
				.actionTraceCallback =
				[this, triggerCueId = std::string(trigger.cueId)](
				const size_t                                       actionIndex,
				const EventPresentationExecutor::ActionTraceStatus status
			) {
					if (!mGraphEditorState || !mGraphEditorState->ui) {
						return;
					}
					EventPresentationEditorGraphUi::RuntimeTraceState uiState =
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

	AudioFxControllerComponent* EventPresentationComponent::ResolveAudioFx() {
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

	CameraFxControllerComponent* EventPresentationComponent::ResolveCameraFx() {
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

	SkeletalAnimationComponent* EventPresentationComponent::ResolveAnimation() {
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
		static constexpr std::string_view kStableName = "game.EventPresentation";
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
