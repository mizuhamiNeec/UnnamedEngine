#pragma once
#include <engine/gameframework/component/base/BaseComponent.h>
#include <runtime/core/math/Math.h>

namespace Unnamed {
	/// @class RotatorComponent
	/// @brief エンティティを自動的に回転させるコンポーネントです。
	class RotatorComponent : public BaseComponent {
	public:
		RotatorComponent();
		~RotatorComponent() override = default;

		//---------------------------------------------------------------------
		// BaseComponent
		//---------------------------------------------------------------------
		void OnAttached() override;
		void OnTick(float deltaTime) override;

		void Serialize(JsonWriter& writer) const override;
		void Deserialize(const JsonReader& reader) override;

		[[nodiscard]] std::string_view GetComponentName() const override;

		//---------------------------------------------------------------------
		// RotatorComponent
		//---------------------------------------------------------------------

		/// @brief 回転速度を設定します（度/秒）
		void SetRotationRate(const Vec3& rotationRate);

		/// @brief 回転速度を取得します（度/秒）
		[[nodiscard]] const Vec3& GetRotationRate() const noexcept;

		/// @brief 回転を有効/無効にします
		void SetRotationEnabled(bool enabled);

		/// @brief 回転が有効かどうかを取得します
		[[nodiscard]] bool IsRotationEnabled() const noexcept;

		/// @brief ピッチ回転を個別に有効/無効にします
		void SetPitchEnabled(bool enabled);

		/// @brief ヨー回転を個別に有効/無効にします
		void SetYawEnabled(bool enabled);

		/// @brief ロール回転を個別に有効/無効にします
		void SetRollEnabled(bool enabled);

	private:
		Vec3 mRotationRate = Vec3::zero; // 回転速度（度/秒）

		bool mRotationEnabled = true; // 回転を有効にするか
		bool mPitchEnabled    = true; // X軸（ピッチ）回転を有効にするか
		bool mYawEnabled      = true; // Y軸（ヨー）回転を有効にするか
		bool mRollEnabled     = true; // Z軸（ロール）回転を有効にするか

		class TransformComponent* mTransform = nullptr; // トランスフォームコンポーネントへの参照
	};
}
