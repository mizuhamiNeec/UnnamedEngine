#pragma once
#include <memory>
#include <core/containers/RingBuffer.h>

#include "core/math/Math.h"
#include "core/math/Vec3.h"

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	class GameMovementStateMachine;
	class TransformComponent;
	class BaseKinematicCollisionResolver;

	struct MovementFrameInput {
		// 移動入力のベクトル
		Vec3 moveAxis = Vec3::zero;

		// 制御中のカメラの基底ベクトル
		Vec3 forward = Vec3::forward;
		Vec3 right   = Vec3::right;
		Vec3 up      = Vec3::up;

		// ジャンプ、しゃがみ、スプリントの入力状態
		bool jumpPressed   = false;
		bool crouchPressed = false;
		bool sprintPressed = false;

		bool noclip = false;
	};

	/// @brief 再現性のある入力パケット
	struct DeterministicInputPacket {
		uint64_t           tick        = 0;    // 入力が適用されるべきシミュレーションのティック
		float              stepSeconds = 0.0f; // シミュレーションステップの時間（秒）
		MovementFrameInput input       = {};   /// 入力情報
	};

	/// @brief 衝突付きでの移動を処理するコンポーネントの基底クラスです。
	class BaseCharacterComponent : public BaseComponent {
	public:
		/// @brief deterministic入力キューの最大保持件数です。
		static constexpr size_t kDeterministicInputQueueCapacity = 128;

		using BaseComponent::BaseComponent;
		~BaseCharacterComponent() override;

		// ---- BaseCharacterComponent ----------------------------------------
		/// @brief 現在のフレームの入力を追加します。
		/// @param input 追加する入力情報
		/// @details プレイヤー、BOT、リプレイなど、様々な入力ソースから呼び出されることを想定しています。
		void AddMovementFrameInput(const MovementFrameInput& input);

		/// @brief 再現性のある入力をキューに追加します。
		/// @param packet 追加する入力パケット
		void EnqueueDeterministicInput(const DeterministicInputPacket& packet);

		/// @brief 再現性のある入力をキューに追加します。
		/// @param tick 入力が適用されるべきシミュレーションのティック
		/// @param stepSeconds シミュレーションステップの時間（秒）
		/// @param input 入力情報
		void EnqueueDeterministicInput(
			uint64_t tick, float stepSeconds, const MovementFrameInput& input
		);

		/// @brief 再現性のある入力をキューから取得します。
		/// @param outPacket 取得した入力パケットの出力先
		/// @return 取得に成功したかどうか
		[[nodiscard]] bool TryDequeueDeterministicInput(
			DeterministicInputPacket& outPacket
		);

		/// @brief 再現性のある入力キューをクリアします。
		void ClearDeterministicInputQueue();

		/// @brief 再現性のある入力キューのサイズを取得します。
		[[nodiscard]] size_t GetDeterministicInputQueueSize() const;

		/// @brief 現在使用中の衝突ハル半径（m）を取得します。
		[[nodiscard]] Vec3 GetCollisionBoxHalfExtents() const noexcept {
			return mBoxHalfExtents;
		}

		/// @brief 1ステップ分の移動シミュレーションを行います。
		/// @param transform
		/// @param input 現在のフレームの入力情報
		/// @param stepSeconds シミュレーションする時間（秒）
		virtual void SimulateStep(
			TransformComponent*       transform,
			const MovementFrameInput& input, float stepSeconds
		);

		// ---- BaseComponent ------------------------------------------------
		void OnAttached() override;

		void PrePhysicsTick(float deltaTime) override;
		void OnTick(float deltaTime) override;
		void PostPhysicsTick(float deltaTime) override;

		[[nodiscard]] std::string_view GetStableName() const override;

		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		uint32_t GetIcon() const override;

	protected:
		[[nodiscard]] virtual TransformComponent* GetTransform() const;

		std::unique_ptr<GameMovementStateMachine> mStateMachine;

		std::unique_ptr<BaseKinematicCollisionResolver> mCollisionResolver;

		MovementFrameInput mMoveFrameInput;
		
		RingBuffer<DeterministicInputPacket, kDeterministicInputQueueCapacity>
		mDeterministicInputQueue;

		Vec3 mVelocity = Vec3::zero; // キャラクターの現在の速度

		bool mGrounded         = false;
		bool mCollisionEnabled = true;

		Vec3 mBoxHalfExtents = Vec3::zero;

		// プレイヤーによって制御されているかどうか
		bool mIsPlayerControlled = true;
	};
}
