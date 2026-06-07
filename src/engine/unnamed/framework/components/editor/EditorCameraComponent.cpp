#include "EditorCameraComponent.h"

#include <imgui.h>

#include "../TransformComponent.h"

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/subsystem/input/device/mouse/MouseDevice.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "EdCamComp";
	static constexpr float            kMoveSpeedPopupDurationSeconds = 1.0f;

	namespace {
		float ReadFloatOr(
			const JsonReader& reader, const char* key, const float fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetFloat() : fallback;
		}
	}

	void EditorCameraComponent::OnAttached() {
		mInput = ServiceLocator::Get<InputSystem>();
		if (!mInput) {
			Error(
				kChannel,
				"UInputSystemを取得できませんでした。EditorCameraComponentは入力を処理できません。"
			);
		}

		constexpr InputKey mouseX = {
			.device = InputDeviceType::MOUSE,
			.code   = VM_X
		};
		constexpr InputKey mouseY = {
			.device = InputDeviceType::MOUSE,
			.code   = VM_Y
		};

		mInput->BindAxis2D(
			"Mouse",
			mouseX,
			INPUT_AXIS::X,
			1.0f
		);

		mInput->BindAxis2D(
			"Mouse",
			mouseY,
			INPUT_AXIS::Y,
			1.0f
		);

		mConsole = ServiceLocator::Get<ConsoleSystem>();
		if (!mConsole) {
			Error(
				kChannel,
				"ConsoleSystemを取得できませんでした。EditorCameraComponentはコンソールにアクセスできません。"
			);
		}
	}

	void EditorCameraComponent::OnEditorTick(float deltaTime) {
		// まずは入力を処理して、カメラの向きと移動方向を更新します。
		mMoveInput = Vec3::zero;
		mWishDir   = Vec3::zero;
		if (mLookEnabled && mInput->
		    IsHeld("ed_look")) {
			ProcessInput();
		} else {
			mInput->SetMouseCursorLocked(false);
			mInput->ClearMouseCursorLockAnchor();
		}

		// 次に、移動速度を更新して、カメラの位置を移動させます。
		auto* transform = GetTransform();
		if (!transform) {
			return;
		}

		if (mOpenPopup) {
			mPopupTimer += deltaTime;
			if (mPopupTimer >= kMoveSpeedPopupDurationSeconds) {
				mOpenPopup = false;
			}
		}

		Friction(6.0f, deltaTime);

		Accelerate(mWishDir, mMoveSpeed, 20.0f, deltaTime);

		Vec3 pos = transform->GetPosition();
		pos      += mVelocity * deltaTime;

		transform->SetPosition(pos);
	}

	void EditorCameraComponent::Deserialize(const JsonReader& reader) {
		mFovYDegrees = ReadFloatOr(reader, "fovYDegrees", mFovYDegrees);
		mNearZ       = ReadFloatOr(reader, "nearZ", mNearZ);
		mFarZ        = ReadFloatOr(reader, "farZ", mFarZ);
		mExposureEv  = ReadFloatOr(reader, "exposureEv", mExposureEv);
		mMoveSpeed   = ReadFloatOr(reader, "moveSpeed", mMoveSpeed);
	}

	void EditorCameraComponent::Serialize(JsonWriter& writer) const {
		writer.Key("fovYDegrees");
		writer.Write(mFovYDegrees);
		writer.Key("nearZ");
		writer.Write(mNearZ);
		writer.Key("farZ");
		writer.Write(mFarZ);
		writer.Key("exposureEv");
		writer.Write(mExposureEv);
		writer.Key("moveSpeed");
		writer.Write(mMoveSpeed);
	}

#ifdef _DEBUG
	void EditorCameraComponent::DrawInspectorImGui() {
		ImGui::DragFloat("FovYDegrees", &mFovYDegrees, 0.1f, 1.0f, 179.0f);
		ImGui::DragFloat("NearZ", &mNearZ, 0.0005f, 0.0001f, mFarZ - 0.001f);
		ImGui::DragFloat("FarZ", &mFarZ, 1.0f, mNearZ + 0.001f, 1000000.0f);
		ImGui::DragFloat("ExposureEV", &mExposureEv, 0.05f, -16.0f, 16.0f);
		ImGui::DragFloat("MoveSpeed", &mMoveSpeed, 1.0f, 1.0f, 10000.0f);
		ImGui::Text("AspectRatio: %.3f", mAspectRatio);
	}
#endif

	void EditorCameraComponent::SetAspectRatio(const float aspectRatio) {
		mAspectRatio = aspectRatio > 0.0f ? aspectRatio : 16.0f / 9.0f;
	}

	void EditorCameraComponent::SetLookEnabled(const bool enabled) noexcept {
		mLookEnabled = enabled;
	}

	bool EditorCameraComponent::IsLookEnabled() const noexcept {
		return mLookEnabled;
	}

	float EditorCameraComponent::GetMoveSpeed() const noexcept {
		return mMoveSpeed;
	}

	bool EditorCameraComponent::IsMoveSpeedPopupVisible() const noexcept {
		return mOpenPopup;
	}

	float EditorCameraComponent::GetMoveSpeedPopupTimer() const noexcept {
		return mPopupTimer;
	}

	bool EditorCameraComponent::BuildViewProjectionMatrices(
		Mat4& outView, Mat4& outProj
	) const {
		const auto* transform = GetTransform();
		if (!transform) {
			return false;
		}

		const Vec3       position = transform->GetPosition();
		const Quaternion rotation = transform->GetRotation();
		const Mat4       world    =
			Mat4::FromQuaternion(rotation) *
			Mat4::Scale(Vec3::one) *
			Mat4::Translate(position);

		outView = world.Inverse();
		outProj = Mat4::PerspectiveFovD3D(
			mFovYDegrees * Math::deg2Rad,
			mAspectRatio,
			mNearZ,
			mFarZ,
			ProjectionDepthMode::ReverseZ
		);
		return true;
	}

	bool EditorCameraComponent::BuildCameraInput(
		Render::RenderCameraInput& outCamera
	) const {
		const auto* transform = GetTransform();
		if (!transform) {
			return false;
		}

		Mat4 view = Mat4::identity;
		Mat4 proj = Mat4::identity;
		if (!BuildViewProjectionMatrices(view, proj)) {
			return false;
		}

		outCamera.view      = view;
		outCamera.proj      = proj;
		outCamera.viewProj  = view * proj;
		outCamera.cameraPos = transform->GetPosition();
		outCamera.exposureEv = mExposureEv;
		outCamera.nearZ     = mNearZ;
		outCamera.farZ      = mFarZ;
		outCamera.depthMode = Render::PROJECTION_DEPTH_MODE::ReverseZ;
		outCamera.valid     = true;
		return true;
	}

	/// @brief 値を最も近い2のべき乗に丸めます。例えば、300は256に、400は512になります。
	/// @param value 丸める値
	/// @return 最も近い2のべき乗に丸められた値
	static float RoundToNearestPowerOfTwo(const float value) {
		const float lowerPowerOfTwo = std::pow(
			2.0f, std::floor(std::log2(value))
		);
		const float upperPowerOfTwo = std::pow(
			2.0f, std::ceil(std::log2(value))
		);
		if (value - lowerPowerOfTwo < upperPowerOfTwo - value) {
			return lowerPowerOfTwo;
		}

		return upperPowerOfTwo;
	}

	void EditorCameraComponent::ProcessInput() {
		if (!mInput) {
			return;
		}

		auto* transform = GetTransform();
		if (!transform) {
			return;
		}

		// マウスカーソルをロック（毎フレーム強制するとコンソールのトグルが効かないため、必要時のみ）
		if (!mInput->IsMouseCursorLocked()) {
			mInput->SetMouseCursorLocked(true);
		}

		// 回転はここで決定する
		const float sensitivity = mConsole->GetConVarValueOr(
			"sensitivity",
			1.0f
		);
		const float pitch = mConsole->GetConVarValueOr("m_pitch", 0.022f);
		const float yaw   = mConsole->GetConVarValueOr("m_yaw", 0.022f);
		const float pitchDown = mConsole->GetConVarValueOr(
			"cl_pitchdown",
			89.0f
		);
		const float pitchUp = mConsole->GetConVarValueOr("cl_pitchup", 89.0f);

		const Vec2 delta = mInput->Axis2D("Mouse");

		mRotation.x += delta.y * sensitivity * pitch;
		mRotation.y += delta.x * sensitivity * yaw;

		mRotation.x = std::clamp(mRotation.x, -pitchDown, pitchUp);

		const Quaternion pitchRotation = Quaternion::AxisAngle(
			Vec3::right, mRotation.x * Math::deg2Rad
		);
		const Quaternion yawRotation = Quaternion::AxisAngle(
			Vec3::up, mRotation.y * Math::deg2Rad
		);

		const Quaternion rotation = yawRotation * pitchRotation;
		transform->SetRotation(rotation);

		// 移動入力の処理
		if (mInput->IsHeld("ed_forward")) {
			mMoveInput.z += 1.0f;
		}
		if (mInput->IsHeld("ed_backward")) {
			mMoveInput.z -= 1.0f;
		}
		if (mInput->IsHeld("ed_left")) {
			mMoveInput.x -= 1.0f;
		}
		if (mInput->IsHeld("ed_right")) {
			mMoveInput.x += 1.0f;
		}
		if (mInput->IsHeld("ed_up")) {
			mMoveInput.y += 1.0f;
		}
		if (mInput->IsHeld("ed_down")) {
			mMoveInput.y -= 1.0f;
		}

		if (mInput->IsPressed("ed_speedup")) {
			mMoveSpeed *= 2.0f;
			mMoveSpeed = RoundToNearestPowerOfTwo(mMoveSpeed);
		}

		if (mInput->IsPressed("ed_slowdown")) {
			mMoveSpeed *= 0.5f;
			mMoveSpeed = RoundToNearestPowerOfTwo(mMoveSpeed);
		}

		if (mMoveSpeed != mOldMoveSpeed) {
			mOpenPopup  = true;
			mPopupTimer = 0.0f;
		}

		mMoveSpeed    = std::clamp(mMoveSpeed, 0.125f, 65535.0f);
		mOldMoveSpeed = mMoveSpeed;

		const Quaternion currentRot = transform->GetRotation();
		const Mat4       rotMatrix  = Mat4::FromQuaternion(currentRot);

		mWishDir = rotMatrix.GetRight() * mMoveInput.x +
		           rotMatrix.GetUp() * mMoveInput.y +
		           rotMatrix.GetForward() * mMoveInput.z;

		mWishDir.Normalize();
	}

	void EditorCameraComponent::Friction(
		const float amount,
		const float deltaTime
	) {
		const float speed = Math::MtoH(mVelocity.Length());

		const float stop = mConsole->GetConVarValueOr("sv_stopspeed", 100.0f);

		const float ctrl = speed < stop ? stop : speed;
		const float drop = ctrl * amount * deltaTime;

		float newSpeed = std::max(0.0f, speed - drop);

		if (newSpeed != speed) {
			newSpeed  /= speed;
			mVelocity *= newSpeed;
		}
	}

	void EditorCameraComponent::Accelerate(
		const Vec3  dir, const float speed, const float accel,
		const float deltaTime
	) {
		if (dir.IsZero() || speed <= 0.0f || accel <= 0.0f) {
			return;
		}
		const float cur = Math::MtoH(mVelocity).Dot(dir);
		const float add = speed - cur;
		if (add <= 0.f) {
			return;
		}
		const float acc = std::min(accel * speed * deltaTime, add);
		mVelocity       += Math::HtoM(acc) * dir;
	}

	TransformComponent* EditorCameraComponent::GetTransform() const {
		Entity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

	REGISTER_COMPONENT(EditorCameraComponent);
}
