#pragma once
#include <cstdint>
#include <string>
#include <string_view>

#include <json.hpp>

#include "base/BaseCharacterComponent.h"
#include "movement/MovementTypes.h"

namespace Unnamed::Physics {
	class Engine;
}

namespace Unnamed {
	class TransformComponent;
	class InputSystem;
	class ConsoleSystem;
	template <typename T>
	class ConVar;

	/// @brief プレイヤーの移動を処理するコンポーネントです。
	class GameMovementComponent : public BaseCharacterComponent {
	public:
		~GameMovementComponent() override;

		// ---- GameMovementComponent -----------------------------------------
		void SimulateStep(
			TransformComponent* transform, const MovementFrameInput& input,
			float               stepSeconds
		) override;

		// ---- BaseComponent ------------------------------------------------
		void OnAttached() override;

		void PrePhysicsTick(float deltaTime) override;
		void OnTick(float deltaTime) override;
		void PostPhysicsTick(float deltaTime) override;

		void OnEditorTick(float deltaTime) override;

		[[nodiscard]] TICK_GROUP GetTickGroup() const override {
			return TICK_GROUP::GAMEPLAY;
		}

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		/// @brief 指定座標へテレポートし、移動状態を安全にリセットします。
		/// @param transform 対象Transform
		/// @param worldPosition テレポート先ワールド座標
		virtual void TeleportAndResetMotion(
			TransformComponent* transform,
			const Vec3&         worldPosition
		);

		/// @brief 動的支持面の任意点速度を取得します。
		[[nodiscard]] Vec3 SampleSupportContactVelocity(
			uint64_t    supportEntityGuid,
			const Vec3& worldPoint
		) const;

		virtual void WriteReplayState(nlohmann::json& outState) const;
		virtual void ReadReplayState(const nlohmann::json& inState);
		[[nodiscard]] virtual uint64_t ComputeReplayStateHash() const;
		/// @brief 地上移動の基準速度を常にスプリント値へ固定するかを返します。
		[[nodiscard]] virtual bool UseSprintSpeedAsDefaultGroundSpeed() const;

	protected:
		/// @brief Locomotion Modeを登録します。
		virtual void RegisterMovementModes(
			GameMovementStateMachine& stateMachine
		);

		/// @brief Movement Abilityを登録します。
		virtual void RegisterMovementAbilities(
			GameMovementStateMachine& stateMachine
		);

		/// @brief 初期Modeを返します。
		[[nodiscard]] virtual MOVEMENT_MODE_ID GetInitialMode() const;

		/// @brief ノークリップ解除後に戻る空中Modeを返します。
		[[nodiscard]] virtual MOVEMENT_MODE_ID GetAirModeForTransitions() const;

		/// @brief 表示/Cue向けの現在移動状態ラベルを解決します。
		[[nodiscard]] virtual std::string ResolvePresentationStateName() const;

		virtual void UpdateCollisionHull(TransformComponent* transform) const;
		[[nodiscard]] MovementCapabilitySet& GetCapabilitySet();
		[[nodiscard]] const MovementCapabilitySet& GetCapabilitySet() const;

		[[nodiscard]] TransformComponent* GetTransform() const override;
		[[nodiscard]] Vec3 GetSupportSamplePoint(
			const TransformComponent* transform
		) const;
		[[nodiscard]] Vec3                ResolveSupportLinearVelocity(
			uint64_t supportEntityGuid
		) const;
		[[nodiscard]] Vec3 ResolveSupportAngularVelocity(
			uint64_t supportEntityGuid
		) const;
		[[nodiscard]] Vec3 ResolveSupportContactVelocity(
			uint64_t    supportEntityGuid,
			const Vec3& worldPoint
		) const;
		[[nodiscard]] Vec3 ResolveSupportStepDelta(
			uint64_t supportEntityGuid, float stepSeconds
		) const;
		[[nodiscard]] bool ApplyPassiveMotionStep(
			TransformComponent* transform, float stepSeconds
		);
		/// @brief GameplayCue を発火します。
		/// @details value/value2 の意味は Cue ごとに異なるため、発火箇所で必ず契約コメントを残します。
		void PublishCue(
			std::string id, float value = 0.0f, float value2 = 0.0f
		);
		virtual void OnAfterCoreCueDispatch(
			std::string_view          previousStateName,
			std::string_view          currentStateName,
			const MovementFrameInput& input,
			bool                      wasGrounded,
			bool                      isGrounded,
			float                     stepSeconds
		);

		Physics::Engine* mPhysics = nullptr;
		InputSystem*     mInput   = nullptr;
		ConsoleSystem*   mConsole = nullptr;

		struct SupportCache {
			bool     grounded              = false;
			uint64_t supportEntityGuid     = 0;
			Vec3     supportLinearVelocity = Vec3::zero;
			Vec3     supportStepDelta      = Vec3::zero;
		};

		SupportCache              mSupportCache;
		float                     mJumpSnapDisableRemaining = 0.0f;
		MovementCapabilitySet     mCapabilitySet = {};
		MOVEMENT_MODE_ID          mCurrentModeId = MOVEMENT_MODE_ID::AIR;
		uint64_t                  mActiveAbilityMask = 0;
		static constexpr uint32_t kMovementRuntimeVersion = 3;

#ifdef _DEBUG
		std::string mDebugLastPublishedCueId;
		float       mDebugLastPublishedCueValue  = 0.0f;
		float       mDebugLastPublishedCueValue2 = 0.0f;
		uint64_t    mDebugPublishedCueCount      = 0;
#endif
		float mCollisionDebugLogCooldownSec = 0.0f;
	};
}
