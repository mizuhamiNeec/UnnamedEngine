#include "SequenceDirectorComponent.h"

#include <algorithm>

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
#include <imgui.h>
#include "engine/ImGui/ImGuiWidgets.h"
#endif

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/types/SequenceAssetData.h"
#include "core/content/ContentPathResolver.h"
#include "core/filesystem/Path.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/Icons.h"
#include "engine/scene/Scene.h"
#include "engine/scene/SceneLoadOptions.h"
#include "engine/sequence/SequencePlayer.h"
#include "engine/sequence/SequenceRuntime.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel = "SequenceDirector";

	}

	void SequenceDirectorComponent::OnAttached() {
		if (
			mSequencePath.has_value() &&
			mSequenceAssetId == kInvalidAssetID
		) {
			if (AssetManager* assetManager = GetAssetManager()) {
				const VirtualPath sequencePath = *mSequencePath;
				(void)SetSequencePath(sequencePath, *assetManager);
			}
		}
		mPlayRequested =
			mPlayOnAttach && mSequenceAssetId != kInvalidAssetID;
	}

	void SequenceDirectorComponent::OnDetached() {
		StopPlayback(true);
		// Runtime側は weak_ptr 登録なので、所有を外せば次ティックで自然に掃除されます。
		mPlayer.reset();
		mSequenceAssetId = kInvalidAssetID;
	}

	void SequenceDirectorComponent::OnTick(const float deltaTime) {
		(void)deltaTime;
		TickDirector();
	}

	void SequenceDirectorComponent::OnEditorTick(const float deltaTime) {
		(void)deltaTime;
		const World* world = GetWorld();
		if (!world || !world->IsGameSimulationEnabled()) {
			// PIE外ではディレクター駆動を止め、ロジックとの競合を防ぎます。
			if (mPlayer && mPlayer->IsEvaluating()) {
				StopPlayback(true);
			}
			return;
		}
		TickDirector();
	}

	std::string_view SequenceDirectorComponent::GetStableName() const {
		return "engine.SequenceDirector";
	}

	std::string_view SequenceDirectorComponent::GetComponentName() const {
		return "SequenceDirector";
	}

	uint32_t SequenceDirectorComponent::GetIcon() const {
		return kIconVideoCam;
	}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	void SequenceDirectorComponent::DrawInspectorImGui() {
		std::string sequencePath = mSequencePath.has_value() ?
			mSequencePath->String() : std::string{};
		const std::string previousSequencePath = sequencePath;
		(void)ImGuiWidgets::InputText<256>("Sequence Path", sequencePath);
		if (
			ImGui::IsItemDeactivatedAfterEdit() &&
			sequencePath != previousSequencePath
		) {
			const std::optional<VirtualPath> virtualPath =
				VirtualPath::ParseContentReference(sequencePath);
			AssetManager* assetManager = GetAssetManager();
			if (!virtualPath.has_value() || !assetManager) {
				ClearSequencePath();
			} else {
				(void)SetSequencePath(*virtualPath, *assetManager);
			}
		}
		ImGui::Checkbox("Play On Attach", &mPlayOnAttach);
		ImGui::Checkbox("Auto Stop When Completed", &mAutoStopWhenCompleted);
		ImGui::DragFloat("Play Rate", &mPlayRate, 0.05f, 0.0f, 8.0f);
		ImGui::Checkbox("Loop", &mLoop);
		int completionMode = static_cast<int>(mCompletionMode);
		if (ImGui::Combo("Completion Mode", &completionMode,
		                 "Restore\0Keep\0")) {
			mCompletionMode = completionMode == 1 ?
				                  SEQUENCE_COMPLETION_MODE::KEEP_STATE :
				                  SEQUENCE_COMPLETION_MODE::RESTORE_STATE;
		}
		ImGui::Checkbox("Apply Component Locks", &mApplyComponentLocks);

		if (ImGui::Button("Add Lock Target")) {
			mLockTargets.emplace_back();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Lock Targets")) {
			mLockTargets.clear();
		}

		for (size_t i = 0; i < mLockTargets.size(); ++i) {
			ImGui::PushID(static_cast<int>(i));
			LockTargetSpec& spec = mLockTargets[i];
			ImGui::InputScalar(
				"Component Guid",
				ImGuiDataType_U64,
				&spec.componentGuid
			);
			ImGui::InputScalar("Entity Guid", ImGuiDataType_U64,
			                   &spec.entityGuid);
			(void)ImGuiWidgets::InputText<128>(
				"Component StableName", spec.componentStableName
			);
			if (ImGui::Button("Remove")) {
				mLockTargets.erase(
					mLockTargets.begin() + static_cast<ptrdiff_t>(i));
				ImGui::PopID();
				break;
			}
			ImGui::Separator();
			ImGui::PopID();
		}

		ImGui::Text("Evaluating: %s",
		            mPlayer && mPlayer->IsEvaluating() ? "true" : "false");
		if (!mPlayer || !mPlayer->IsEvaluating()) {
			if (ImGui::Button("Start")) {
				mPlayRequested = true;
			}
		} else if (ImGui::Button("Stop")) {
			StopPlayback(true);
		}
	}
#endif

	void SequenceDirectorComponent::Deserialize(const JsonReader& reader) {
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

	bool SequenceDirectorComponent::Deserialize(
		const JsonReader& reader, const SceneDeserializeContext& context
	) {
		ClearSequencePath();
		if (const JsonReader playOnAttach = reader["playOnAttach"];
			playOnAttach.Valid()) {
			mPlayOnAttach = playOnAttach.GetBool(mPlayOnAttach);
		}
		if (const JsonReader autoStop = reader["autoStopWhenCompleted"];
			autoStop.Valid()) {
			mAutoStopWhenCompleted = autoStop.GetBool(mAutoStopWhenCompleted);
		}
		if (const JsonReader playRate = reader["playRate"];
			playRate.Valid()) {
			mPlayRate = std::max(0.0f, playRate.GetFloat(mPlayRate));
		}
		if (const JsonReader loop = reader["loop"];
			loop.Valid()) {
			mLoop = loop.GetBool(mLoop);
		}
		if (const JsonReader completionMode = reader["completionMode"];
			completionMode.Valid()) {
			const std::string text = completionMode.GetString("Restore");
			mCompletionMode        = text == "Keep" ?
				                  SEQUENCE_COMPLETION_MODE::KEEP_STATE :
				                  SEQUENCE_COMPLETION_MODE::RESTORE_STATE;
		}
		if (const JsonReader applyLocks = reader["applyComponentLocks"];
			applyLocks.Valid()) {
			mApplyComponentLocks = applyLocks.GetBool(mApplyComponentLocks);
		}

		mLockTargets.clear();
		if (const JsonReader lockTargets = reader["lockTargets"];
			lockTargets.Valid()) {
			const JsonReader lockArray = lockTargets.GetArray();
			for (size_t i = 0; i < lockArray.Size(); ++i) {
				const JsonReader lockNode = lockArray[i];
				if (!lockNode.Valid()) {
					continue;
				}
				LockTargetSpec spec = {};
				if (const JsonReader componentGuid = lockNode["componentGuid"];
					componentGuid.Valid()) {
					spec.componentGuid = componentGuid.GetUint64();
				}
				if (const JsonReader entityGuid = lockNode["entityGuid"];
					entityGuid.Valid()) {
					spec.entityGuid = entityGuid.GetUint64();
				}
				spec.componentStableName = lockNode["componentStableName"].
					GetString(
						spec.componentStableName
					);
				mLockTargets.emplace_back(std::move(spec));
			}
		}

		const JsonReader pathNode = reader["sequencePath"];
		if (!pathNode.Valid()) {
			return true;
		}
		if (!pathNode.IsString()) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='invalid virtual path' scene='{}' entity='{}' entityId={} component='{}' field='sequencePath' reason='expected string'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType
			);
			return !IsStrictAssetValidation(context.loadOptions);
		}

		const std::string sequencePath = pathNode.GetString();
		if (sequencePath.empty()) {
			return true;
		}
		const std::optional<VirtualPath> virtualPath =
			VirtualPath::ParseContentReference(sequencePath);
		if (!virtualPath.has_value()) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='invalid virtual path' scene='{}' entity='{}' entityId={} component='{}' field='sequencePath' virtualPath='{}'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType,
				sequencePath
			);
			return !IsStrictAssetValidation(context.loadOptions);
		}

		if (!context.assetManager) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='sequencePath' virtualPath='{}' reason='AssetManager unavailable'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType,
				sequencePath
			);
			return !IsStrictAssetValidation(context.loadOptions);
		}

		const std::optional<ResolvedContentFile> resolvedFile =
			context.assetManager->GetContentPathResolver().ResolveFile(*virtualPath);
		if (!resolvedFile.has_value()) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='unresolved content' scene='{}' entity='{}' entityId={} component='{}' field='sequencePath' virtualPath='{}'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType,
				sequencePath
			);
			return !IsStrictAssetValidation(context.loadOptions);
		}

		if (SetSequencePath(*virtualPath, *context.assetManager)) {
			return true;
		}
		Error(
			kChannel,
			"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='sequencePath' virtualPath='{}' mount='{}' physicalPath='{}'",
			context.scenePath,
			context.entityName,
			context.entityId,
			context.componentType,
			sequencePath,
			resolvedFile->mountId,
			resolvedFile->resolvedPath
		);
		return !IsStrictAssetValidation(context.loadOptions);
	}

	void SequenceDirectorComponent::Serialize(JsonWriter& writer) const {
		if (mSequencePath.has_value()) {
			writer.Key("sequencePath");
			writer.Write(mSequencePath->String());
		}
		writer.Key("playOnAttach");
		writer.Write(mPlayOnAttach);
		writer.Key("autoStopWhenCompleted");
		writer.Write(mAutoStopWhenCompleted);
		writer.Key("playRate");
		writer.Write(mPlayRate);
		writer.Key("loop");
		writer.Write(mLoop);
		writer.Key("completionMode");
		writer.Write(
			mCompletionMode == SEQUENCE_COMPLETION_MODE::KEEP_STATE ?
				"Keep" :
				"Restore"
		);
		writer.Key("applyComponentLocks");
		writer.Write(mApplyComponentLocks);

		writer.Key("lockTargets");
		writer.BeginArray();
		for (const LockTargetSpec& spec : mLockTargets) {
			writer.BeginObject();
			writer.Key("componentGuid");
			writer.Write(spec.componentGuid);
			writer.Key("entityGuid");
			writer.Write(spec.entityGuid);
			writer.Key("componentStableName");
			writer.Write(spec.componentStableName);
			writer.EndObject();
		}
		writer.EndArray();
	}

	bool SequenceDirectorComponent::SetSequencePath(
		const VirtualPath& path, AssetManager& assetManager
	) {
		if (
			mSequencePath.has_value() && *mSequencePath == path &&
			mSequenceAssetId != kInvalidAssetID
		) {
			return true;
		}

		const AssetID assetId = assetManager.LoadSequence(path);
		if (assetId == kInvalidAssetID) {
			ClearSequencePath();
			return false;
		}

		StopPlayback(true);
		mSequencePath      = path;
		mSequenceAssetId   = assetId;
		mLoggedLoadFailure = false;
		mPlayRequested     = mPlayOnAttach;
		return true;
	}

	void SequenceDirectorComponent::ClearSequencePath() {
		StopPlayback(true);
		mSequencePath.reset();
		mSequenceAssetId   = kInvalidAssetID;
		mLoggedLoadFailure = false;
	}

	const std::optional<VirtualPath>&
	SequenceDirectorComponent::GetSequencePath() const noexcept {
		return mSequencePath;
	}

	AssetID SequenceDirectorComponent::GetSequenceAssetId() const noexcept {
		return mSequenceAssetId;
	}

	void SequenceDirectorComponent::EnsurePlayer() {
		if (!mPlayer) {
			mPlayer = std::make_shared<SequencePlayer>();
		}

		if (World* world = GetWorld()) {
			world->GetSequenceRuntime().RegisterPlayer(mPlayer);
		}
	}

	bool SequenceDirectorComponent::StartPlayback() {
		const World* world = GetWorld();
		if (!world) {
			return false;
		}
		AssetManager* assetManager = world->GetAssetManager();
		if (!assetManager) {
			return false;
		}
		if (
			!mSequencePath.has_value() ||
			mSequenceAssetId == kInvalidAssetID
		) {
			return false;
		}

		EnsurePlayer();
		if (!mPlayer) {
			return false;
		}

		const auto* sequenceAsset = assetManager->Get<SequenceAssetData>(
			mSequenceAssetId);
		if (!sequenceAsset) {
			if (!mLoggedLoadFailure) {
				Warning(kChannel, "Failed to load sequence '{}'.",
				        mSequencePath->String());
				mLoggedLoadFailure = true;
			}
			return false;
		}

		mLoggedLoadFailure = false;
		mPlayer->SetAssetId(mSequenceAssetId);
		mPlayer->SetPlayRate(std::max(0.0f, mPlayRate));
		mPlayer->SetLoop(mLoop);
		mPlayer->SetCompletionMode(mCompletionMode);
		mPlayer->Play();
		mPlayRequested = false;
		return true;
	}

	void SequenceDirectorComponent::StopPlayback(const bool restoreState) {
		if (mPlayer) {
			mPlayer->Stop();
		}
		if (restoreState) {
			RestoreLockTargets();
		}
		mPlayRequested = false;
		mWasEvaluating = false;
	}

	void SequenceDirectorComponent::TickDirector() {
		EnsurePlayer();
		if (!mPlayer) {
			return;
		}

		if (mPlayRequested && !mPlayer->IsEvaluating()) {
			(void)StartPlayback();
		}

		if (
			mAutoStopWhenCompleted &&
			mPlayer->GetState() == SEQUENCE_PLAYER_STATE::COMPLETED &&
			mCompletionMode == SEQUENCE_COMPLETION_MODE::RESTORE_STATE
		) {
			StopPlayback(true);
		}

		const bool evaluating = mPlayer->IsEvaluating();
		if (evaluating && !mWasEvaluating) {
			ApplyLockTargets();
		}
		if (!evaluating && mWasEvaluating) {
			RestoreLockTargets();
		}
		mWasEvaluating = evaluating;
	}

	void SequenceDirectorComponent::ApplyLockTargets() {
		RestoreLockTargets();
		if (!mApplyComponentLocks) {
			return;
		}

		std::vector<BaseComponent*> lockedComponents = {};
		lockedComponents.reserve(mLockTargets.size());
		for (const LockTargetSpec& spec : mLockTargets) {
			BaseComponent* target = ResolveLockTarget(spec);
			if (!target || target == this) {
				continue;
			}
			if (
				std::ranges::find(lockedComponents, target) !=
				lockedComponents.end()
			) {
				continue;
			}

			mActiveLocks.emplace_back(
				ActiveLockState{
					.component      = target,
					.previousActive = target->IsActive(),
				}
			);
			lockedComponents.emplace_back(target);
			target->SetActive(false);
		}
	}

	void SequenceDirectorComponent::RestoreLockTargets() {
		for (const ActiveLockState& lockState : mActiveLocks) {
			if (!lockState.component) {
				continue;
			}
			lockState.component->SetActive(lockState.previousActive);
		}
		mActiveLocks.clear();
	}

	BaseComponent* SequenceDirectorComponent::ResolveLockTarget(
		const LockTargetSpec& spec
	) const {
		Scene* scene = GetScene();
		if (!scene) {
			return nullptr;
		}

		auto matchesSpec = [&](BaseComponent& component) {
			if (
				spec.componentGuid != 0 &&
				component.GetGuid() != spec.componentGuid
			) {
				return false;
			}
			if (
				!spec.componentStableName.empty() &&
				component.GetStableName() != spec.componentStableName
			) {
				return false;
			}
			return true;
		};

		auto findInEntity = [&](Entity& entity) -> BaseComponent* {
			BaseComponent* found = nullptr;
			entity.ForEachComponent(
				[&](BaseComponent& component) {
					if (!matchesSpec(component)) {
						return true;
					}
					found = &component;
					return false;
				}
			);
			return found;
		};

		if (spec.entityGuid != 0) {
			if (Entity* entity = scene->FindEntity(spec.entityGuid)) {
				return findInEntity(*entity);
			}
			return nullptr;
		}

		if (!spec.componentStableName.empty()) {
			if (Entity* owner = GetOwner()) {
				if (BaseComponent* found = findInEntity(*owner)) {
					return found;
				}
			}
		}

		for (const auto& entityPtr : scene->GetEntities()) {
			if (!entityPtr) {
				continue;
			}
			if (BaseComponent* found = findInEntity(*entityPtr)) {
				return found;
			}
		}

		return nullptr;
	}

	REGISTER_COMPONENT(SequenceDirectorComponent);
}
