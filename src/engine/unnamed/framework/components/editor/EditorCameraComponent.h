#pragma once
#include "../base/BaseComponent.h"

#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"

namespace Unnamed {
	class TransformComponent;
	class JsonReader;
	class JsonWriter;

	class EditorCameraComponent final : public BaseComponent {
	public:
		// ---- EditorCameraComponent -----------------------------------------
		void SetAspectRatio(float aspectRatio);

		[[nodiscard]] float GetAspectRatio() const {
			return mAspectRatio;
		}

		void SetLookEnabled(bool enabled) noexcept;
		[[nodiscard]] bool IsLookEnabled() const noexcept;
		[[nodiscard]] float GetMoveSpeed() const noexcept;
		[[nodiscard]] bool IsMoveSpeedPopupVisible() const noexcept;
		[[nodiscard]] float GetMoveSpeedPopupTimer() const noexcept;
		bool BuildViewProjectionMatrices(Mat4& outView, Mat4& outProj) const;

		bool BuildCameraInput(Render::RenderCameraInput& outCamera) const;

		// ---- BaseComponent ------------------------------------------------
		void OnAttached() override;

		void OnEditorTick(float deltaTime) override;

		[[nodiscard]] std::string_view GetStableName() const override {
			return "engine.EditorCamera";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "EditorCamera";
		}

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		/// @brief 入力を処理し、移動方向を更新します。
		void ProcessInput();

		/// @brief 摩擦を適用して速度を減衰させます。
		/// @param amount 摩擦の強さ。大きいほど速く減衰します。
		/// @param deltaTime 前のフレームからの経過時間 [秒]
		void Friction(float amount, float deltaTime);

		/// @brief 空中での加速処理を行います。
		/// @param dir プレイヤーが移動したい方向の単位ベクトル
		/// @param speed プレイヤーが移動したい速度
		/// @param accel 加速率
		/// @param deltaTime 前のフレームからの経過時間 [秒]
		void Accelerate(
			Vec3 dir, float speed, float accel, float deltaTime
		);

		/// @brief TransformComponentを取得します。存在しない場合はnullptrを返します。
		/// @return TransformComponentへのポインタ、存在しない場合はnullptr
		[[nodiscard]] TransformComponent* GetTransform() const;

		InputSystem*         mInput   = nullptr;
		class ConsoleSystem* mConsole = nullptr;

		Vec3 mMoveInput = Vec3::zero;
		Vec3 mWishDir   = Vec3::zero;
		Vec3 mVelocity  = Vec3::zero;

		Vec3 mRotation = Vec3::zero; // pitch (x), yaw (y), roll (z)

		float mFovYDegrees = 90.0f;
		float mNearZ       = 0.001f;
		float mFarZ        = 10000.0f;
		float mExposureEv  = 0.0f;
		float mRotateSpeed = 1.5f;
		float mAspectRatio = 16.0f / 9.0f;

		float mMoveSpeed    = 256.0f;
		float mOldMoveSpeed = 256.0f;
		float mPopupTimer   = 0.0f;

		bool mLookEnabled = false;
		bool mOpenPopup   = false;
	};
}
