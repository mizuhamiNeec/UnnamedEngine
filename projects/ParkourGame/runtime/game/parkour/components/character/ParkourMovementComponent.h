#pragma once

#include <json.hpp>
#include <core/containers/RingBuffer.h>

#include "GrappleMotor.h"

#include "game/core/components/character/GameMovementComponent.h"
#include "game/core/input/CharacterActionFrameInput.h"

namespace Unnamed {
	struct MovementContext;

	/// @brief パルクール向けの拡張移動コンポーネントです。
	class ParkourMovementComponent final : public GameMovementComponent,
	                                       public
	                                       ICharacterActionInputReceiver {
	public:
		/// @brief deterministicアクション入力キューの最大保持件数です。
		static constexpr size_t kDeterministicActionInputQueueCapacity = 128;

		struct WallRunRuntime {
			bool  active            = false;
			Vec3  normal            = Vec3::zero;
			Vec3  direction         = Vec3::zero;
			float time              = 0.0f;
			float timeSinceLast     = 999.0f;
			Vec3  lastWallNormal    = Vec3::zero;
			bool  jumpWasHeldOnInit = false;
		};

		struct SlideRuntime {
			bool  active    = false;
			Vec3  direction = Vec3::zero;
			float time      = 0.0f;
		};

		struct BlinkRuntime {
			float cooldown  = 0.0f;
			bool  active    = false;
			float moveTime  = 0.0f;
			Vec3  startPos  = Vec3::zero;
			Vec3  targetPos = Vec3::zero;
		};

		struct VaultRuntime {
			bool  active      = false;
			float time        = 0.0f;
			float cooldown    = 0.0f;
			Vec3  startPos    = Vec3::zero;
			Vec3  apexPos     = Vec3::zero;
			Vec3  endPos      = Vec3::zero;
			Vec3  preVelocity = Vec3::zero;
		};

		struct ParkourRuntime {
			bool           hasDoubleJump           = true;
			bool           lastJumpHeld            = false;
			bool           duckHullActive          = false;
			bool           pendingDoubleJumpCue    = false;
			float          footstepDistanceAccumHu = 0.0f;
			WallRunRuntime wallRun                 = {};
			SlideRuntime   slide                   = {};
			BlinkRuntime   blink                   = {};
			VaultRuntime   vault                   = {};

			GrappleState grapple = {};
		};

		~ParkourMovementComponent() override;

		void OnAttached() override;
		void SimulateStep(
			TransformComponent*       transform,
			const MovementFrameInput& input,
			float                     stepSeconds
		) override;

		void EnqueueDeterministicActionInput(
			uint64_t                         tick,
			float                            stepSeconds,
			const CharacterActionFrameInput& input
		) override;
		void ClearDeterministicActionInputQueue() override;

		[[nodiscard]] const CharacterActionFrameInput& GetActionFrameInput()
		const;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;
		void WriteReplayState(nlohmann::json& outState) const override;
		void ReadReplayState(const nlohmann::json& inState) override;
		[[nodiscard]] uint64_t ComputeReplayStateHash() const override;

		[[nodiscard]] uint32_t GetIcon() const override;

		[[nodiscard]] ParkourRuntime&       GetParkourRuntime();
		[[nodiscard]] const ParkourRuntime& GetParkourRuntime() const;

		void TickParkourTimers(float deltaTime);
		/// @brief Parkour移動Abilityから共通Cueを発火します。
		void PublishParkourMovementCue(
			std::string_view id,
			float            value  = 0.0f,
			float            value2 = 0.0f
		);

		/// @brief しゃがみハルの有効/無効を切り替えます。
		bool SetDuckHullEnabled(MovementContext& context, bool enabled);
		/// @brief 現在しゃがみハルを使用中かどうかを返します。
		[[nodiscard]] bool IsDuckHullEnabled() const;
		/// @brief 現在位置から「立ち」へ遷移可能かを判定します。
		[[nodiscard]] bool CanStandAt(const MovementContext& context) const;

		[[nodiscard]] float GetHorizontalSpeedHu(const Vec3& velocity) const;
		[[nodiscard]] bool  ShouldSlideFromSpeed(float horizontalSpeedHu) const;
		[[nodiscard]] bool  WantsDuckStance(
			const MovementContext& context
		) const;
		void SyncCollisionHull(TransformComponent* transform);
		void EndWallRun();

	protected:
		void RegisterMovementModes(
			GameMovementStateMachine& stateMachine
		) override;
		void RegisterMovementAbilities(
			GameMovementStateMachine& stateMachine
		) override;

		/// @brief Parkour移動の初期Modeを返します。
		[[nodiscard]] MOVEMENT_MODE_ID GetInitialMode() const override;

		/// @brief ノークリップ解除後に戻るParkour空中状態IDを返します。
		[[nodiscard]] MOVEMENT_MODE_ID
		GetAirModeForTransitions() const override;
		[[nodiscard]] std::string ResolvePresentationStateName() const override;
		/// @brief Parkour地上移動は常時スプリント基準にするためtrueを返します。
		[[nodiscard]] bool UseSprintSpeedAsDefaultGroundSpeed() const override;

		void OnAfterCoreCueDispatch(
			std::string_view          previousStateName,
			std::string_view          currentStateName,
			const MovementFrameInput& input,
			bool                      wasGrounded,
			bool                      isGrounded,
			float                     stepSeconds
		) override;

	private:
		/// @brief 1回のハル占有判定で収集したデバッグ情報です。
		struct HullOccupancyDebugInfo {
			bool  checkSweepPath       = false;
			Vec3  sweepStartCenter     = Vec3::zero;
			Vec3  sweepEndCenter       = Vec3::zero;
			Vec3  sweepHalfExtents     = Vec3::zero;
			bool  sweepHit             = false;
			bool  sweepBlocked         = false;
			float sweepHitToi          = 1.0f;
			Vec3  sweepHitPos          = Vec3::zero;
			Vec3  sweepHitNormal       = Vec3::zero;
			Vec3  sweepReachableCenter = Vec3::zero;

			bool overlapBlocked   = false;
			Vec3 overlapHitPos    = Vec3::zero;
			Vec3 overlapHitNormal = Vec3::zero;
		};

		/// @brief Duck/UnDuck判定を可視化するフレームデータです。
		struct DuckStandDebugFrame {
			bool evaluateStandCalled = false;
			bool standAllowed        = false;
			bool grounded            = false;

			Vec3 currentCenter          = Vec3::zero;
			Vec3 currentHalfExtents     = Vec3::zero;
			Vec3 standTargetCenter      = Vec3::zero;
			Vec3 standTargetHalfExtents = Vec3::zero;

			HullOccupancyDebugInfo standOccupancy = {};

			bool  headSweepUsed            = false;
			Vec3  headSweepStartCenter     = Vec3::zero;
			Vec3  headSweepHalfExtents     = Vec3::zero;
			float headSweepLength          = 0.0f;
			bool  headSweepBlocked         = false;
			float headSweepHitToi          = 1.0f;
			Vec3  headSweepHitPos          = Vec3::zero;
			Vec3  headSweepHitNormal       = Vec3::zero;
			Vec3  headSweepReachableCenter = Vec3::zero;

			bool standApplyAttempted     = false;
			bool standApplySucceeded     = false;
			Vec3 standAppliedCenter      = Vec3::zero;
			Vec3 standAppliedHalfExtents = Vec3::zero;

			bool duckApplyAttempted     = false;
			bool duckApplySucceeded     = false;
			Vec3 duckAppliedCenter      = Vec3::zero;
			Vec3 duckAppliedHalfExtents = Vec3::zero;
		};

		void RebuildDuckHalfExtents();
		void ResetParkourRuntime();
		/// @brief Duck視点補間のランタイム状態を初期化します。
		void ResetDuckViewRuntime();
		/// @brief Duck状態に応じてカメラルートの視点高さを更新します。
		void UpdateDuckViewHeight(
			TransformComponent*       actorTransform,
			const MovementFrameInput& input,
			float                     stepSeconds
		);
		/// @brief プレイヤー配下のカメラルートTransformを解決します。
		[[nodiscard]] TransformComponent* ResolveCameraRootTransform(
			TransformComponent* actorTransform
		);
		/// @brief Duck/UnDuckデバッグ表示を有効にするかを返します。
		[[nodiscard]] bool IsDuckDebugDrawEnabled() const;
		/// @brief 1フレーム分のDuck/UnDuckデバッグ状態をリセットします。
		void ResetDuckStandDebugFrame();
		/// @brief Duck/UnDuckの判定状態をデバッグ描画します。
		void DrawDuckStandDebug(TransformComponent* transform) const;
		/// @brief 地上/空中の状態に応じたハル中心オフセットを返します。
		[[nodiscard]] Vec3 ComputeHullCenterOffsetForDuckState(
			const MovementContext& context,
			bool                   toDuck
		) const;
		/// @brief しゃがみ用の接地判定フラグを返します。
		[[nodiscard]] bool IsDuckGrounded(const MovementContext& context) const;
		/// @brief 指定ハルを配置できるか（経路スイープ込み）を判定します。
		[[nodiscard]] bool CanOccupyHull(
			const MovementContext&  context,
			const Vec3&             targetCenter,
			const Vec3&             targetHalfExtents,
			bool                    checkSweepPath,
			HullOccupancyDebugInfo* outDebugInfo = nullptr
		) const;
		bool ApplyDuckHull(MovementContext& context);
		bool ApplyStandHull(MovementContext& context);

		bool                        mAutoSprintActive = false;
		ParkourRuntime              mRuntime = {};
		mutable DuckStandDebugFrame mDuckStandDebug = {};
		Vec3                        mStandingHalfExtents = Vec3::zero;
		Vec3                        mDuckHalfExtents = Vec3::zero;
		uint64_t                    mCameraRootEntityGuid = 0;
		Vec3                        mCameraRootBaseLocalPosition = Vec3::zero;
		float                       mStandViewHeightHu = 64.0f;
		float                       mDuckViewHeightHu = 28.0f;
		float                       mDuckViewTimeToDuckSec = 0.4f;
		float                       mDuckViewTimeToUnduckSec = 0.2f;
		float                       mDuckViewClampPaddingHu = 1.0f;
		float                       mDuckViewFraction = 0.0f;
		float                       mDuckViewUnduckHoldRemainingSec = 0.0f;
		float                       mDuckViewSmoothedLocalY = 0.0f;
		bool                        mDuckViewFractionInitialized = false;
		bool                        mDuckViewSmoothedLocalYInitialized = false;
		bool                        mCameraRootBaseLocalCached = false;
		CharacterActionFrameInput   mActionFrameInput = {};

		struct DeterministicActionInputPacket {
			uint64_t                  tick        = 0;
			float                     stepSeconds = 0.0f;
			CharacterActionFrameInput input       = {};
		};

		RingBuffer<
			DeterministicActionInputPacket,
			kDeterministicActionInputQueueCapacity
		>
		mDeterministicActionInputQueue;
	};
}
