#include "CourseProgressComponent.h"

#include <algorithm>
#include <array>
#include <cstring>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Math.h"
#include "core/math/Quaternion.h"
#include "core/math/Vec4.h"

#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/world/GameplayCueBus.h"
#include "engine/world/World.h"

#include "game/core/components/character/GameMovementComponent.h"
#include "game/core/components/character/base/BaseCharacterComponent.h"
#include "game/core/replay/ReplayHash.h"
#include "game/parkour/components/trigger/CheckpointComponent.h"
#include "game/parkour/components/trigger/GoalComponent.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] bool IsAabbOverlapping(
			const Vec3& minA,
			const Vec3& maxA,
			const Vec3& minB,
			const Vec3& maxB
		) {
			return minA.x <= maxB.x && maxA.x >= minB.x &&
			       minA.y <= maxB.y && maxA.y >= minB.y &&
			       minA.z <= maxB.z && maxA.z >= minB.z;
		}

		[[nodiscard]] std::string BuildCourseCueId(
			const std::string_view courseId,
			const std::string_view suffix
		) {
			const std::string normalizedCourse =
				courseId.empty() ?
					std::string("default") :
					std::string(courseId);
			return "course." + normalizedCourse + "." + std::string(suffix);
		}

		[[nodiscard]] bool TryReadVec3(
			const nlohmann::json& object,
			const char*           key,
			Vec3&                 outValue
		) {
			if (!object.is_object()) {
				return false;
			}
			const auto it = object.find(key);
			if (it == object.end() || !it->is_array() || it->size() != 3) {
				return false;
			}
			outValue = Vec3(
				(*it)[0].get<float>(),
				(*it)[1].get<float>(),
				(*it)[2].get<float>()
			);
			return true;
		}
	}

	void CourseProgressComponent::OnAttached() {
		ClearDeterministicActionInputQueue();
		mActionFrameInput = {};
		ResetProgress();
	}

	void CourseProgressComponent::OnTick(const float deltaTime) {
		DeterministicActionInputPacket actionPacket = {};
		if (mDeterministicActionInputQueue.Pop(actionPacket)) {
			mActionFrameInput = actionPacket.input;
		} else {
			mActionFrameInput = {};
		}

		TransformComponent* transform = GetOwner() ?
			                                GetOwner()->GetComponent<
				                                TransformComponent>() :
			                                nullptr;
		if (mReloadRespawnEnabled && mActionFrameInput.weapon.reloadPressed) {
			RespawnToLatest(transform);
		}
		TickCourseProgress(transform, std::max(0.0f, deltaTime), false);
	}

	void CourseProgressComponent::OnEditorTick(const float deltaTime) {
		(void)deltaTime;
		TransformComponent* transform = GetOwner() ?
			                                GetOwner()->GetComponent<
				                                TransformComponent>() :
			                                nullptr;
		TickCourseProgress(transform, 0.0f, true);
	}

	std::string_view CourseProgressComponent::GetStableName() const {
		return "parkour.CourseProgress";
	}

	std::string_view CourseProgressComponent::GetComponentName() const {
		return "CourseProgress";
	}

#ifdef _DEBUG
	void CourseProgressComponent::DrawInspectorImGui() {
		std::array<char, 64> courseIdBuffer  = {};
		const size_t         courseIdCopyLen = std::min(
			mCourseId.size(),
			courseIdBuffer.size() - 1
		);
		if (courseIdCopyLen > 0) {
			std::memcpy(
				courseIdBuffer.data(), mCourseId.data(), courseIdCopyLen
			);
		}
		if (ImGui::InputText(
			"Course Id",
			courseIdBuffer.data(),
			courseIdBuffer.size()
		)) {
			mCourseId = courseIdBuffer.data();
			if (mCourseId.empty()) {
				mCourseId = "default";
			}
		}
		ImGui::Checkbox("Reload Respawn Enabled", &mReloadRespawnEnabled);
		ImGui::Checkbox("HUD Enabled", &mHudEnabled);
		ImGui::Checkbox("Debug Draw", &mDebugDraw);
		ImGui::DragFloat3(
			"Fallback Half Extents HU",
			&mFallbackPlayerHalfExtentsHu.x,
			0.5f,
			0.0f,
			1024.0f
		);

		ImGui::SeparatorText("Course Snapshot");
		ImGui::Text(
			"Spawn Initialized: %s",
			mSnapshot.spawnInitialized ? "true" : "false"
		);
		ImGui::Text("Next Checkpoint Index: %d", mSnapshot.nextCheckpointIndex);
		ImGui::Text("Last Checkpoint Index: %d", mSnapshot.lastCheckpointIndex);
		ImGui::Text(
			"Checkpoints Passed: %d / %d",
			static_cast<int>(mSnapshot.touchedCheckpointIndices.size()),
			static_cast<int>(mSnapshot.orderedCheckpointIndices.size())
		);
		ImGui::Text(
			"All Checkpoints Passed: %s",
			mSnapshot.allCheckpointsPassed ? "true" : "false"
		);
		ImGui::Text(
			"Course Cleared: %s", mSnapshot.courseCleared ? "true" : "false"
		);
		ImGui::Text("Elapsed Seconds: %.2f", mSnapshot.elapsedSeconds);
		ImGui::Text(
			"Cleared Elapsed Seconds: %.2f",
			mSnapshot.clearedElapsedSeconds
		);
		if (ImGui::Button("Reset Progress")) {
			ResetProgress();
		}
	}
#endif

	void CourseProgressComponent::Deserialize(const JsonReader& reader) {
		mCourseId = reader["courseId"].GetString(mCourseId);
		if (mCourseId.empty()) {
			mCourseId = "default";
		}
		if (const JsonReader reloadRespawn = reader["reloadRespawnEnabled"];
			reloadRespawn.Valid()) {
			mReloadRespawnEnabled = reloadRespawn.GetBool(
				mReloadRespawnEnabled
			);
		}
		if (const JsonReader hudEnabled = reader["hudEnabled"];
			hudEnabled.Valid()) {
			mHudEnabled = hudEnabled.GetBool(mHudEnabled);
		}
		if (const JsonReader debugDraw = reader["debugDraw"];
			debugDraw.Valid()) {
			mDebugDraw = debugDraw.GetBool(mDebugDraw);
		}
		if (const JsonReader fallbackHalfExtents = reader[
				"fallbackHalfExtentsHu"];
			fallbackHalfExtents.Valid()) {
			mFallbackPlayerHalfExtentsHu = fallbackHalfExtents.GetVec3(
				mFallbackPlayerHalfExtentsHu
			);
		}
		ResetProgress();
	}

	void CourseProgressComponent::Serialize(JsonWriter& writer) const {
		writer.Key("courseId");
		writer.Write(mCourseId);
		writer.Key("reloadRespawnEnabled");
		writer.Write(mReloadRespawnEnabled);
		writer.Key("hudEnabled");
		writer.Write(mHudEnabled);
		writer.Key("debugDraw");
		writer.Write(mDebugDraw);
		writer.WriteVec3("fallbackHalfExtentsHu", mFallbackPlayerHalfExtentsHu);
	}

	void CourseProgressComponent::EnqueueDeterministicActionInput(
		const uint64_t                   tick,
		const float                      stepSeconds,
		const CharacterActionFrameInput& input
	) {
		mDeterministicActionInputQueue.Push(
			DeterministicActionInputPacket{
				.tick        = tick,
				.stepSeconds = stepSeconds,
				.input       = input
			}
		);
	}

	void CourseProgressComponent::ClearDeterministicActionInputQueue() {
		mDeterministicActionInputQueue.Clear();
	}

	const CourseProgressSnapshot& CourseProgressComponent::GetSnapshot() const {
		return mSnapshot;
	}

	std::string_view CourseProgressComponent::GetCourseId() const {
		return mSnapshot.courseId;
	}

	bool CourseProgressComponent::IsHudEnabled() const {
		return mHudEnabled;
	}

	bool CourseProgressComponent::ResolveCurrentRespawnPosition(
		Vec3& outPosition
	) const {
		if (!mSnapshot.spawnInitialized) {
			return false;
		}
		outPosition = mSnapshot.lastCheckpointIndex >= 0 ?
			              mSnapshot.respawnPosition :
			              mSnapshot.spawnPosition;
		return true;
	}

	void CourseProgressComponent::WriteReplayState(
		nlohmann::json& outState
	) const {
		outState                     = nlohmann::json::object();
		outState["courseId"]         = mCourseId;
		outState["spawnInitialized"] = mSnapshot.spawnInitialized;
		outState["spawnPosition"]    = nlohmann::json::array(
			{
				mSnapshot.spawnPosition.x, mSnapshot.spawnPosition.y,
				mSnapshot.spawnPosition.z
			}
		);
		outState["respawnPosition"] = nlohmann::json::array(
			{
				mSnapshot.respawnPosition.x, mSnapshot.respawnPosition.y,
				mSnapshot.respawnPosition.z
			}
		);
		outState["nextCheckpointIndex"]      = mSnapshot.nextCheckpointIndex;
		outState["lastCheckpointIndex"]      = mSnapshot.lastCheckpointIndex;
		outState["allCheckpointsPassed"]     = mSnapshot.allCheckpointsPassed;
		outState["courseCleared"]            = mSnapshot.courseCleared;
		outState["elapsedSeconds"]           = mSnapshot.elapsedSeconds;
		outState["clearedElapsedSeconds"]    = mSnapshot.clearedElapsedSeconds;
		outState["orderedCheckpointIndices"] = mSnapshot.
			orderedCheckpointIndices;
		outState["touchedCheckpointIndices"] = mSnapshot.
			touchedCheckpointIndices;
	}

	void CourseProgressComponent::ReadReplayState(
		const nlohmann::json& inState
	) {
		if (!inState.is_object()) {
			return;
		}

		mCourseId = inState.value("courseId", mCourseId);
		if (mCourseId.empty()) {
			mCourseId = "default";
		}
		mSnapshot.courseId         = mCourseId;
		mSnapshot.spawnInitialized = inState.value(
			"spawnInitialized",
			mSnapshot.spawnInitialized
		);
		(void)TryReadVec3(inState, "spawnPosition", mSnapshot.spawnPosition);
		(void)TryReadVec3(
			inState, "respawnPosition", mSnapshot.respawnPosition
		);
		mSnapshot.nextCheckpointIndex = inState.value(
			"nextCheckpointIndex",
			mSnapshot.nextCheckpointIndex
		);
		mSnapshot.lastCheckpointIndex = inState.value(
			"lastCheckpointIndex",
			mSnapshot.lastCheckpointIndex
		);
		mSnapshot.allCheckpointsPassed = inState.value(
			"allCheckpointsPassed",
			mSnapshot.allCheckpointsPassed
		);
		mSnapshot.courseCleared = inState.value(
			"courseCleared",
			mSnapshot.courseCleared
		);
		mSnapshot.elapsedSeconds = inState.value(
			"elapsedSeconds",
			mSnapshot.elapsedSeconds
		);
		mSnapshot.clearedElapsedSeconds = inState.value(
			"clearedElapsedSeconds",
			mSnapshot.clearedElapsedSeconds
		);

		if (const auto itOrdered = inState.find("orderedCheckpointIndices");
			itOrdered != inState.end() && itOrdered->is_array()) {
			mSnapshot.orderedCheckpointIndices.clear();
			for (const auto& value : *itOrdered) {
				if (!value.is_number_integer()) {
					continue;
				}
				mSnapshot.orderedCheckpointIndices.emplace_back(
					value.get<int32_t>()
				);
			}
		}

		if (const auto itTouched = inState.find("touchedCheckpointIndices");
			itTouched != inState.end() && itTouched->is_array()) {
			mSnapshot.touchedCheckpointIndices.clear();
			for (const auto& value : *itTouched) {
				if (!value.is_number_integer()) {
					continue;
				}
				mSnapshot.touchedCheckpointIndices.emplace_back(
					value.get<int32_t>()
				);
			}
		}

		mTouchedCheckpointIndices.clear();
		for (const int32_t index : mSnapshot.touchedCheckpointIndices) {
			mTouchedCheckpointIndices.insert(index);
		}
	}

	uint64_t CourseProgressComponent::ComputeReplayStateHash() const {
		uint64_t hash = ReplayHash::Begin();
		ReplayHash::AppendString(hash, mCourseId);
		ReplayHash::AppendPod(hash, mSnapshot.spawnInitialized);
		ReplayHash::AppendFloating(hash, mSnapshot.spawnPosition.x);
		ReplayHash::AppendFloating(hash, mSnapshot.spawnPosition.y);
		ReplayHash::AppendFloating(hash, mSnapshot.spawnPosition.z);
		ReplayHash::AppendFloating(hash, mSnapshot.respawnPosition.x);
		ReplayHash::AppendFloating(hash, mSnapshot.respawnPosition.y);
		ReplayHash::AppendFloating(hash, mSnapshot.respawnPosition.z);
		ReplayHash::AppendPod(hash, mSnapshot.nextCheckpointIndex);
		ReplayHash::AppendPod(hash, mSnapshot.lastCheckpointIndex);
		ReplayHash::AppendPod(hash, mSnapshot.allCheckpointsPassed);
		ReplayHash::AppendPod(hash, mSnapshot.courseCleared);
		ReplayHash::AppendFloating(hash, mSnapshot.elapsedSeconds);
		ReplayHash::AppendFloating(hash, mSnapshot.clearedElapsedSeconds);

		ReplayHash::AppendPod(
			hash,
			static_cast<uint32_t>(mSnapshot.orderedCheckpointIndices.size())
		);
		for (const int32_t index : mSnapshot.orderedCheckpointIndices) {
			ReplayHash::AppendPod(hash, index);
		}
		ReplayHash::AppendPod(
			hash,
			static_cast<uint32_t>(mSnapshot.touchedCheckpointIndices.size())
		);
		for (const int32_t index : mSnapshot.touchedCheckpointIndices) {
			ReplayHash::AppendPod(hash, index);
		}
		return hash;
	}

	void CourseProgressComponent::ResetProgress() {
		mTouchedCheckpointIndices.clear();
		mSnapshot          = {};
		mSnapshot.courseId = mCourseId.empty() ? "default" : mCourseId;
	}

	void CourseProgressComponent::EnsureSpawnInitialized(
		const TransformComponent* transform
	) {
		if (mSnapshot.spawnInitialized || !transform) {
			return;
		}
		mSnapshot.spawnPosition    = transform->GetPosition();
		mSnapshot.respawnPosition  = mSnapshot.spawnPosition;
		mSnapshot.spawnInitialized = true;
	}

	void CourseProgressComponent::TickCourseProgress(
		TransformComponent* transform,
		const float         deltaTime,
		const bool          drawDebug
	) {
		if (!transform) {
			return;
		}

		EnsureSpawnInitialized(transform);
		if (!mSnapshot.courseCleared) {
			mSnapshot.elapsedSeconds += std::max(0.0f, deltaTime);
		}
		World* world = transform->GetWorld();
		if (!world) {
			return;
		}

		const Scene&                       scene       = world->GetScene();
		std::vector<CourseTriggerSnapshot> checkpoints = {};
		std::vector<CourseTriggerSnapshot> goals       = {};
		checkpoints.reserve(16);
		goals.reserve(8);

		const std::string activeCourseId = mCourseId.empty() ?
			                                   std::string("default") :
			                                   mCourseId;

		for (const auto& entityPtr : scene.GetEntities()) {
			if (!entityPtr || !entityPtr->IsActive()) {
				continue;
			}
			Entity* entity = entityPtr.get();

			if (const auto* checkpoint = entity->GetComponent<
					CheckpointComponent>();
				checkpoint && checkpoint->IsActive() &&
				checkpoint->GetCourseId() == activeCourseId) {
				checkpoints.emplace_back(
					CourseTriggerSnapshot{
						.kind             = COURSE_TRIGGER_KIND::CHECKPOINT,
						.entityGuid       = entity->GetGuid(),
						.index            = checkpoint->GetIndex(),
						.worldCenter      = checkpoint->GetWorldCenter(),
						.worldHalfExtents = checkpoint->
						GetWorldHalfExtentsMeters(),
						.respawnPosition = checkpoint->GetRespawnPosition(),
						.hasRespawn      = true,
					}
				);
			}

			if (const auto* goal = entity->GetComponent<GoalComponent>();
				goal && goal->IsActive() &&
				goal->GetCourseId() == activeCourseId) {
				goals.emplace_back(
					CourseTriggerSnapshot{
						.kind             = COURSE_TRIGGER_KIND::GOAL,
						.entityGuid       = entity->GetGuid(),
						.index            = goal->GetIndex(),
						.worldCenter      = goal->GetWorldCenter(),
						.worldHalfExtents = goal->GetWorldHalfExtentsMeters(),
						.respawnPosition  = Vec3::zero,
						.hasRespawn       = false,
					}
				);
			}
		}

		std::sort(
			checkpoints.begin(),
			checkpoints.end(),
			[](const CourseTriggerSnapshot& a, const CourseTriggerSnapshot& b) {
				if (a.index != b.index) {
					return a.index < b.index;
				}
				return a.worldCenter.SqrLength() < b.worldCenter.SqrLength();
			}
		);

		mSnapshot.orderedCheckpointIndices.clear();
		mSnapshot.orderedCheckpointIndices.reserve(checkpoints.size());
		for (const CourseTriggerSnapshot& checkpoint : checkpoints) {
			if (mSnapshot.orderedCheckpointIndices.empty() ||
			    mSnapshot.orderedCheckpointIndices.back() != checkpoint.index) {
				mSnapshot.orderedCheckpointIndices.emplace_back(
					checkpoint.index
				);
			}
		}

		mSnapshot.nextCheckpointIndex = ResolveNextCheckpointIndex();

		const Vec3 playerCenter      = transform->GetPosition();
		const Vec3 playerHalfExtents = ResolvePlayerHalfExtentsMeters();
		const Vec3 playerMin         = playerCenter - playerHalfExtents;
		const Vec3 playerMax         = playerCenter + playerHalfExtents;

		bool    checkpointPassedThisFrame = false;
		int32_t passedCheckpointIndex     = -1;
		for (const CourseTriggerSnapshot& checkpoint : checkpoints) {
			const Vec3 triggerMin =
				checkpoint.worldCenter - checkpoint.worldHalfExtents;
			const Vec3 triggerMax =
				checkpoint.worldCenter + checkpoint.worldHalfExtents;
			if (!IsAabbOverlapping(
				playerMin, playerMax, triggerMin, triggerMax
			)) {
				continue;
			}

			if (mTouchedCheckpointIndices.contains(checkpoint.index)) {
				continue;
			}
			if (checkpoint.index != mSnapshot.nextCheckpointIndex) {
				continue;
			}

			// 指定順チェックポイントを通過したときだけ進行を更新します。
			mTouchedCheckpointIndices.insert(checkpoint.index);
			mSnapshot.lastCheckpointIndex = checkpoint.index;
			if (checkpoint.hasRespawn) {
				mSnapshot.respawnPosition = checkpoint.respawnPosition;
			} else {
				mSnapshot.respawnPosition = checkpoint.worldCenter;
			}
			mSnapshot.nextCheckpointIndex = ResolveNextCheckpointIndex();
			checkpointPassedThisFrame     = true;
			passedCheckpointIndex         = checkpoint.index;
		}

		bool allCheckpointsPassed = true;
		for (const int32_t index : mSnapshot.orderedCheckpointIndices) {
			if (!mTouchedCheckpointIndices.contains(index)) {
				allCheckpointsPassed = false;
				break;
			}
		}
		mSnapshot.allCheckpointsPassed = allCheckpointsPassed;

		bool courseClearedThisFrame = false;
		if (!mSnapshot.courseCleared && allCheckpointsPassed) {
			for (const CourseTriggerSnapshot& goal : goals) {
				const Vec3 triggerMin =
					goal.worldCenter - goal.worldHalfExtents;
				const Vec3 triggerMax =
					goal.worldCenter + goal.worldHalfExtents;
				if (!IsAabbOverlapping(
					playerMin, playerMax, triggerMin, triggerMax
				)) {
					continue;
				}
				mSnapshot.courseCleared = true;
				mSnapshot.clearedElapsedSeconds = mSnapshot.elapsedSeconds;
				courseClearedThisFrame  = true;
				break;
			}
		}

		mSnapshot.checkpoints   = checkpoints;
		mSnapshot.goals         = goals;
		mSnapshot.hasNextTarget = false;
		for (const CourseTriggerSnapshot& checkpoint : checkpoints) {
			if (checkpoint.index != mSnapshot.nextCheckpointIndex) {
				continue;
			}
			mSnapshot.nextTargetWorldPosition = checkpoint.worldCenter;
			mSnapshot.hasNextTarget           = true;
			break;
		}
		if (!mSnapshot.hasNextTarget &&
		    mSnapshot.allCheckpointsPassed &&
		    !goals.empty()) {
			mSnapshot.nextTargetWorldPosition = goals.front().worldCenter;
			mSnapshot.hasNextTarget           = true;
		}

		UpdateTouchedSnapshot();
		++mSnapshot.revision;

		if (checkpointPassedThisFrame) {
			PublishCheckpointPassedCue(passedCheckpointIndex);
		}
		if (courseClearedThisFrame) {
			PublishCourseClearedCue();
		}

		if (drawDebug || mDebugDraw) {
			DrawCourseDebug(transform);
		}
	}

	void CourseProgressComponent::DrawCourseDebug(
		TransformComponent* transform
	) const {
		if (!transform) {
			return;
		}
		World* world = transform->GetWorld();
		if (!world) {
			return;
		}

		auto&      debugDraw              = world->GetDebugDraw();
		const Vec4 checkpointDoneColor    = Vec4(0.2f, 0.95f, 0.2f, 1.0f);
		const Vec4 checkpointNextColor    = Vec4(1.0f, 0.85f, 0.2f, 1.0f);
		const Vec4 checkpointPendingColor = Vec4(0.55f, 0.55f, 0.55f, 1.0f);
		const Vec4 respawnLineColor       = Vec4(0.8f, 0.8f, 1.0f, 1.0f);
		const Vec4 goalLockedColor        = Vec4(1.0f, 0.25f, 0.25f, 1.0f);
		const Vec4 goalReadyColor         = Vec4(0.3f, 0.9f, 1.0f, 1.0f);
		const Vec4 goalClearedColor       = Vec4(0.2f, 1.0f, 0.6f, 1.0f);

		for (const CourseTriggerSnapshot& checkpoint : mSnapshot.checkpoints) {
			const bool done = mTouchedCheckpointIndices.contains(
				checkpoint.index
			);
			const bool isNext = !done &&
			                    checkpoint.index == mSnapshot.
			                    nextCheckpointIndex;
			const Vec4 color = done ?
				                   checkpointDoneColor :
				                   isNext ?
				                   checkpointNextColor :
				                   checkpointPendingColor;
			debugDraw.DrawBox(
				checkpoint.worldCenter,
				Quaternion::identity,
				checkpoint.worldHalfExtents * 2.0f,
				color
			);
			if (checkpoint.hasRespawn) {
				debugDraw.DrawLine(
					checkpoint.worldCenter,
					checkpoint.respawnPosition,
					respawnLineColor
				);
			}
		}

		for (const CourseTriggerSnapshot& goal : mSnapshot.goals) {
			const Vec4 color = mSnapshot.courseCleared ?
				                   goalClearedColor :
				                   mSnapshot.allCheckpointsPassed ?
				                   goalReadyColor :
				                   goalLockedColor;
			debugDraw.DrawBox(
				goal.worldCenter,
				Quaternion::identity,
				goal.worldHalfExtents * 2.0f,
				color
			);
		}
	}

	void CourseProgressComponent::UpdateTouchedSnapshot() {
		mSnapshot.touchedCheckpointIndices.clear();
		mSnapshot.touchedCheckpointIndices.reserve(
			mTouchedCheckpointIndices.size()
		);

		for (const int32_t index : mSnapshot.orderedCheckpointIndices) {
			if (mTouchedCheckpointIndices.contains(index)) {
				mSnapshot.touchedCheckpointIndices.emplace_back(index);
			}
		}

		std::vector<int32_t> extras = {};
		for (const int32_t index : mTouchedCheckpointIndices) {
			if (std::ranges::find(mSnapshot.touchedCheckpointIndices, index) !=
			    mSnapshot.touchedCheckpointIndices.end()) {
				continue;
			}
			extras.emplace_back(index);
		}
		std::ranges::sort(extras);
		for (const int32_t extra : extras) {
			mSnapshot.touchedCheckpointIndices.emplace_back(extra);
		}
	}

	void CourseProgressComponent::RespawnToLatest(
		TransformComponent* transform
	) {
		if (!transform) {
			return;
		}

		EnsureSpawnInitialized(transform);
		const Vec3 targetPosition = mSnapshot.lastCheckpointIndex >= 0 ?
			                            mSnapshot.respawnPosition :
			                            mSnapshot.spawnPosition;

		if (Entity* owner = GetOwner()) {
			if (auto* movement = owner->FindComponentByBase<
				GameMovementComponent>()) {
				movement->TeleportAndResetMotion(transform, targetPosition);
				return;
			}
		}

		// 移動コンポーネントが無いケースはTransformだけをリセットします。
		transform->SetPosition(targetPosition);
		transform->RequestInterpolationResync();
	}

	int32_t CourseProgressComponent::ResolveNextCheckpointIndex() const {
		for (const int32_t index : mSnapshot.orderedCheckpointIndices) {
			if (!mTouchedCheckpointIndices.contains(index)) {
				return index;
			}
		}
		return mSnapshot.orderedCheckpointIndices.empty() ?
			       0 :
			       mSnapshot.orderedCheckpointIndices.back() + 1;
	}

	Vec3 CourseProgressComponent::ResolvePlayerHalfExtentsMeters() const {
		Vec3 halfExtentsHu = mFallbackPlayerHalfExtentsHu;
		if (const Entity* owner = GetOwner()) {
			if (const auto* character = owner->FindComponentByBase<
				BaseCharacterComponent>()) {
				halfExtentsHu = character->GetCollisionBoxHalfExtents();
			}
		}
		return Math::HtoM(halfExtentsHu);
	}

	void CourseProgressComponent::PublishCheckpointPassedCue(
		const int32_t checkpointIndex
	) const {
		World*  world = GetWorld();
		Entity* owner = GetOwner();
		if (!world || !owner || owner->GetGuid() == 0) {
			return;
		}

		GameplayCue cue = {};
		cue.id = BuildCourseCueId(mCourseId, "checkpoint.passed");
		cue.sourceEntityGuid = owner->GetGuid();
		cue.value = static_cast<float>(checkpointIndex);
		cue.value2 = static_cast<float>(mSnapshot.nextCheckpointIndex);
		world->GetGameplayCueBus().Publish(cue);
	}

	void CourseProgressComponent::PublishCourseClearedCue() const {
		World*  world = GetWorld();
		Entity* owner = GetOwner();
		if (!world || !owner || owner->GetGuid() == 0) {
			return;
		}

		GameplayCue cue      = {};
		cue.id               = BuildCourseCueId(mCourseId, "cleared");
		cue.sourceEntityGuid = owner->GetGuid();
		cue.value            = 1.0f;
		cue.value2           = mSnapshot.clearedElapsedSeconds;
		world->GetGameplayCueBus().Publish(cue);

		Msg(
			"CourseProgress",
			"Course cleared: course={} elapsed={:.2f}s",
			mCourseId,
			mSnapshot.clearedElapsedSeconds
		);
	}

	REGISTER_COMPONENT(CourseProgressComponent);
}
