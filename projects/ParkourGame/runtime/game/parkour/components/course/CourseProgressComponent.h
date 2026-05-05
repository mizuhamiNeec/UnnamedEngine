#pragma once

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <json.hpp>

#include "core/containers/RingBuffer.h"
#include "core/math/Vec3.h"

#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include "game/core/input/CharacterActionFrameInput.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;
	class TransformComponent;

	/// @brief コーストリガー種別です。
	enum class COURSE_TRIGGER_KIND : uint8_t {
		CHECKPOINT = 0,
		GOAL       = 1,
	};

	/// @brief 1フレームで収集したコーストリガーのスナップショットです。
	struct CourseTriggerSnapshot {
		COURSE_TRIGGER_KIND kind             = COURSE_TRIGGER_KIND::CHECKPOINT;
		uint64_t            entityGuid       = 0;
		int32_t             index            = 0;
		Vec3                worldCenter      = Vec3::zero;
		Vec3                worldHalfExtents = Vec3::zero;
		Vec3                respawnPosition  = Vec3::zero;
		bool                hasRespawn       = false;
	};

	/// @brief 外部参照用のコース進行スナップショットです。
	struct CourseProgressSnapshot {
		std::string                    courseId = "default";
		std::vector<CourseTriggerSnapshot> checkpoints = {};
		std::vector<CourseTriggerSnapshot> goals       = {};
		std::vector<int32_t>           orderedCheckpointIndices = {};
		std::vector<int32_t>           touchedCheckpointIndices = {};
		Vec3                           spawnPosition = Vec3::zero;
		Vec3                           respawnPosition = Vec3::zero;
		Vec3                           nextTargetWorldPosition = Vec3::zero;
		int32_t                        nextCheckpointIndex = 0;
		int32_t                        lastCheckpointIndex = -1;
		bool                           spawnInitialized = false;
		bool                           allCheckpointsPassed = false;
		bool                           courseCleared = false;
		bool                           hasNextTarget = false;
		float                          elapsedSeconds = 0.0f;
		float                          clearedElapsedSeconds = 0.0f;
		uint64_t                       revision = 0;
	};

	/// @brief チェックポイント/ゴール進行を管理するプレイヤー側コンポーネントです。
	class CourseProgressComponent final : public BaseComponent,
	                                      public ICharacterActionInputReceiver {
	public:
		/// @brief deterministicアクション入力キューの最大保持件数です。
		static constexpr size_t kDeterministicActionInputQueueCapacity = 128;

		void OnAttached() override;
		void OnTick(float deltaTime) override;
		void OnEditorTick(float deltaTime) override;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		void EnqueueDeterministicActionInput(
			uint64_t                         tick,
			float                            stepSeconds,
			const CharacterActionFrameInput& input
		) override;
		void ClearDeterministicActionInputQueue() override;

		/// @brief 現在のコース進行スナップショットを返します。
		[[nodiscard]] const CourseProgressSnapshot& GetSnapshot() const;
		/// @brief 進行管理対象のコースIDを返します。
		[[nodiscard]] std::string_view GetCourseId() const;
		/// @brief HUD表示を許可するかどうかを返します。
		[[nodiscard]] bool IsHudEnabled() const;

		/// @brief 現在有効なリスポーン座標を返します。
		[[nodiscard]] bool ResolveCurrentRespawnPosition(Vec3& outPosition) const;

		/// @brief リプレイ用状態を書き込みます。
		void WriteReplayState(nlohmann::json& outState) const;
		/// @brief リプレイ用状態を読み込みます。
		void ReadReplayState(const nlohmann::json& inState);
		/// @brief リプレイ整合確認用ハッシュを計算します。
		[[nodiscard]] uint64_t ComputeReplayStateHash() const;

	private:
		struct DeterministicActionInputPacket {
			uint64_t                  tick        = 0;
			float                     stepSeconds = 0.0f;
			CharacterActionFrameInput input       = {};
		};

		void ResetProgress();
		void EnsureSpawnInitialized(const TransformComponent* transform);
		void TickCourseProgress(
			TransformComponent* transform,
			float               deltaTime,
			bool                drawDebug
		);
		void DrawCourseDebug(TransformComponent* transform) const;
		void UpdateTouchedSnapshot();
		void RespawnToLatest(TransformComponent* transform);
		[[nodiscard]] int32_t ResolveNextCheckpointIndex() const;
		[[nodiscard]] Vec3 ResolvePlayerHalfExtentsMeters() const;
		void PublishCheckpointPassedCue(int32_t checkpointIndex) const;
		void PublishCourseClearedCue() const;

		std::string mCourseId = "default";
		bool        mReloadRespawnEnabled = true;
		bool        mHudEnabled = true;
		bool        mDebugDraw = true;
		Vec3        mFallbackPlayerHalfExtentsHu = Vec3(16.0f, 36.0f, 16.0f);

		CourseProgressSnapshot      mSnapshot = {};
		std::unordered_set<int32_t> mTouchedCheckpointIndices = {};
		CharacterActionFrameInput   mActionFrameInput = {};
		RingBuffer<
			DeterministicActionInputPacket,
			kDeterministicActionInputQueueCapacity
		>
		mDeterministicActionInputQueue;
	};
}
