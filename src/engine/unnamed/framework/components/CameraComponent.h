#pragma once

#include "base/BaseComponent.h"

#include "engine/render/frame/RenderFrameInputs.h"

namespace Unnamed {
	class TransformComponent;
	class JsonReader;
	class JsonWriter;

	class CameraComponent final : public BaseComponent {
	public:
		// ---- CameraComponent -----------------------------------------------
		/// @brief アスペクト比を設定します。
		/// @param aspectRatio 画面の幅を高さで割った値。
		void SetAspectRatio(float aspectRatio);

		/// @brief カメラをアクティブにするかどうかを設定します。アクティブなカメラはレンダリングに使用されます。
		/// @param active trueに設定するとカメラがアクティブになります。falseに設定すると非アクティブになります。
		void SetCameraActive(bool active) noexcept;

		/// @brief アスペクト比を取得します。
		/// @return 画面の幅を高さで割った値。
		[[nodiscard]] float GetAspectRatio() const noexcept;

		/// @brief カメラがアクティブかどうかを取得します。アクティブなカメラはレンダリングに使用されます。
		/// @return カメラがアクティブかどうか。
		[[nodiscard]] bool IsCameraActive() const noexcept;

		/// @brief カメラ入力を構築します。
		/// @param outCamera カメラ入力の構築結果を格納する変数への参照。
		/// @return カメラ入力の構築に成功したかどうか。
		bool BuildCameraInput(Render::RenderCameraInput& outCamera) const;

		void                SetFovYDegrees(float fovYDegrees);
		[[nodiscard]] float GetFovYDegrees() const noexcept;

		[[nodiscard]] bool SetAsCurrentCamera() const;
		[[nodiscard]] bool IsCurrentCamera() const;

		// ---- BaseComponent ------------------------------------------------
		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		[[nodiscard]] uint32_t GetIcon() const override;

	private:
		/// @brief TransformComponentを取得します。存在しない場合はnullptrを返します。
		/// @return TransformComponentへのポインタ、存在しない場合はnullptr
		[[nodiscard]] TransformComponent* GetTransform() const;

		float mFovYDegrees  = 90.0f;
		float mNearZ        = 0.01f;
		float mFarZ         = 10000.0f;
		float mAspectRatio  = 16.0f / 9.0f;
		float mExposureEv   = 0.0f;
		bool  mCameraActive = true;
	};
}
