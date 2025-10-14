#include "RotatorComponent.h"

#include <engine/gameframework/component/Transform/TransformComponent.h>
#include <engine/gameframework/entity/base/BaseEntity.h>
#include <core/json/JsonReader.h>
#include <core/json/JsonWriter.h>

namespace Unnamed {
	RotatorComponent::RotatorComponent() = default;

	/// @brief エンティティに取り付けられた際に呼び出されます。
	void RotatorComponent::OnAttached() {
		// TransformComponentへの参照を取得
		if (mOwner) {
			mTransform = mOwner->GetComponent<TransformComponent>();
		}
	}

	/// @brief 毎フレーム呼び出され、回転を更新します。
	void RotatorComponent::OnTick(float deltaTime) {
		// 回転が無効、またはTransformComponentが無い場合は何もしない
		if (!mRotationEnabled || !mTransform || !mIsActive) {
			return;
		}

		// 回転速度を適用（度/秒からラジアン/秒に変換して適用）
		Vec3 rotationDelta = Vec3::zero;

		if (mPitchEnabled) {
			rotationDelta.x = mRotationRate.x * deltaTime;
		}
		if (mYawEnabled) {
			rotationDelta.y = mRotationRate.y * deltaTime;
		}
		if (mRollEnabled) {
			rotationDelta.z = mRotationRate.z * deltaTime;
		}

		// 回転が実際にある場合のみ適用
		if (rotationDelta.x != 0.0f || rotationDelta.y != 0.0f || rotationDelta.
			z != 0.0f) {
			// 度からラジアンに変換して回転を適用
			Vec3       rotationDeltaRad = rotationDelta * Math::deg2Rad;
			Quaternion deltaRotation    = Quaternion::Euler(rotationDeltaRad);

			// 現在の回転に追加
			Quaternion currentRotation = mTransform->Rotation();
			Quaternion newRotation     = currentRotation * deltaRotation;
			newRotation.Normalize();

			mTransform->SetRotation(newRotation);
		}
	}

	/// @brief コンポーネントをJSONにシリアライズします。
	void RotatorComponent::Serialize(JsonWriter& writer) const {
		writer.BeginObject();

		writer.Key("rotationRate");
		writer.BeginObject();
		writer.Key("x");
		writer.Write(mRotationRate.x);
		writer.Key("y");
		writer.Write(mRotationRate.y);
		writer.Key("z");
		writer.Write(mRotationRate.z);
		writer.EndObject();

		writer.Key("rotationEnabled");
		writer.Write(mRotationEnabled);
		writer.Key("pitchEnabled");
		writer.Write(mPitchEnabled);
		writer.Key("yawEnabled");
		writer.Write(mYawEnabled);
		writer.Key("rollEnabled");
		writer.Write(mRollEnabled);
		writer.Key("isActive");
		writer.Write(mIsActive);

		writer.EndObject();
	}

	/// @brief JSONからコンポーネントをデシリアライズします。
	void RotatorComponent::Deserialize(const JsonReader& reader) {
		if (!reader.Valid()) return;

		if (reader.Has("rotationRate")) {
			auto rate = reader["rotationRate"];
			if (rate.Has("x")) mRotationRate.x = rate["x"].GetFloat();
			if (rate.Has("y")) mRotationRate.y = rate["y"].GetFloat();
			if (rate.Has("z")) mRotationRate.z = rate["z"].GetFloat();
		}

		// if (reader.Has("rotationEnabled")) {
		// 	mRotationEnabled = reader["rotationEnabled"].Get<bool>();
		// }
		// if (reader.Has("pitchEnabled")) {
		// 	mPitchEnabled = reader["pitchEnabled"].Get<bool>();
		// }
		// if (reader.Has("yawEnabled")) {
		// 	mYawEnabled = reader["yawEnabled"].Get<bool>();
		// }
		// if (reader.Has("rollEnabled")) {
		// 	mRollEnabled = reader["rollEnabled"].Get<bool>();
		// }
		// if (reader.Has("isActive")) {
		// 	mIsActive = reader["isActive"].Get<bool>();
		// }
	}

	/// @brief コンポーネント名を取得します。
	std::string_view RotatorComponent::GetComponentName() const {
		return "RotatorComponent";
	}

	/// @brief 回転速度を設定します（度/秒）
	void RotatorComponent::SetRotationRate(const Vec3& rotationRate) {
		mRotationRate = rotationRate;
	}

	/// @brief 回転速度を取得します（度/秒）
	const Vec3& RotatorComponent::GetRotationRate() const noexcept {
		return mRotationRate;
	}

	/// @brief 回転を有効/無効にします
	void RotatorComponent::SetRotationEnabled(bool enabled) {
		mRotationEnabled = enabled;
	}

	/// @brief 回転が有効かどうかを取得します
	bool RotatorComponent::IsRotationEnabled() const noexcept {
		return mRotationEnabled;
	}

	/// @brief ピッチ回転を個別に有効/無効にします
	void RotatorComponent::SetPitchEnabled(bool enabled) {
		mPitchEnabled = enabled;
	}

	/// @brief ヨー回転を個別に有効/無効にします
	void RotatorComponent::SetYawEnabled(bool enabled) {
		mYawEnabled = enabled;
	}

	/// @brief ロール回転を個別に有効/無効にします
	void RotatorComponent::SetRollEnabled(bool enabled) {
		mRollEnabled = enabled;
	}
}
