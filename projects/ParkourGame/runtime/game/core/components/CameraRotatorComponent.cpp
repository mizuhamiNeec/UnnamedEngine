#include "CameraRotatorComponent.h"

#include <imgui.h>

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Math.h"
#include "core/math/Quaternion.h"

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/subsystem/input/device/gamepad/GamepadDevice.h"
#include "engine/unnamed/subsystem/input/device/mouse/MouseDevice.h"

#include "game/core/replay/ReplayHash.h"

namespace Unnamed {
	void CameraRotatorComponent::OnAttached() {
		mInput = GetInputSystem();
		BindLookAxisOnce();
		if (const TransformComponent* transform = GetTransform()) {
			const Vec3 eulerDegrees = transform->GetRotation().ToEulerDegrees();
			mCurrentPitch           = eulerDegrees.x;
			mCurrentYaw             = eulerDegrees.y;
		}

		mConsole = GetConsoleSystem();
	}

	void CameraRotatorComponent::PrePhysicsTick(const float deltaTime) {
		if (!mUseDirectInput) {
			ApplyRotationFromCurrentAngles();
			return;
		}

		Vec2 mouseDelta   = Vec2::zero;
		Vec2 gamepadDelta = Vec2::zero;
		if (mInput) {
			mouseDelta   = mInput->Axis2D("Mouse");
			gamepadDelta = mInput->Axis2D("GamepadLook");
		}

		AddLookInput(mouseDelta, gamepadDelta, deltaTime);
	}

	void CameraRotatorComponent::AddLookInput(
		const Vec2& mouseDelta,
		const Vec2& gamepadDelta,
		const float deltaTime
	) {
		if (!GetTransform() || !mConsole) {
			return;
		}

		// コンソールから各値を取得。
		const float sensitivity = mConsole->GetConVarValueOr(
			"sensitivity", 1.0f
		);
		const float pitch = mConsole->GetConVarValueOr(
			"m_pitch", 0.022f
		);
		const float yaw = mConsole->GetConVarValueOr(
			"m_yaw", 0.022f
		);
		const float pitchUpLimit = mConsole->GetConVarValueOr(
			"cl_pitchup", 89.0f
		);
		const float pitchDownLimit = mConsole->GetConVarValueOr(
			"cl_pitchdown", 89.0f
		);

		const float joySensitivity = mConsole->GetConVarValueOr(
			"joy_sensitivity", 360.0f
		);

		// deltaTimeはゲームパッドの入力にのみ適用する。
		mCurrentPitch += mouseDelta.y * sensitivity * pitch;
		mCurrentYaw   += mouseDelta.x * sensitivity * yaw;

		mCurrentPitch += gamepadDelta.y * joySensitivity * deltaTime;
		mCurrentYaw   += gamepadDelta.x * joySensitivity * deltaTime;

		mCurrentPitch = std::clamp(
			mCurrentPitch, -pitchUpLimit, pitchDownLimit
		);
		if (TransformComponent* transform = GetTransform()) {
			if (!mouseDelta.IsZero() || !gamepadDelta.IsZero()) {
				// 視点の回転はすぐに反映させる。
				transform->RequestInterpolationResync();
			}
		}
		ApplyRotationFromCurrentAngles();
	}

	void CameraRotatorComponent::SetDirectInputEnabled(const bool enabled) {
		mUseDirectInput = enabled;
	}

	bool CameraRotatorComponent::IsDirectInputEnabled() const {
		return mUseDirectInput;
	}

	void CameraRotatorComponent::SetExternalLookOffsetDegrees(
		const Vec2& offsetDegrees
	) {
		mExternalLookOffsetDegrees = offsetDegrees;
		ApplyRotationFromCurrentAngles();
	}

	Vec2 CameraRotatorComponent::GetExternalLookOffsetDegrees() const {
		return mExternalLookOffsetDegrees;
	}

	std::string_view CameraRotatorComponent::GetStableName() const {
		return "game.CameraRotator";
	}

	std::string_view CameraRotatorComponent::GetComponentName() const {
		return "CameraRotator";
	}

	void CameraRotatorComponent::Deserialize(const JsonReader& reader) {
		const JsonReader pitch = reader["pitchDegrees"];
		const JsonReader yaw   = reader["yawDegrees"];
		if (pitch.Valid()) {
			mCurrentPitch = pitch.GetFloat();
		}
		if (yaw.Valid()) {
			mCurrentYaw = yaw.GetFloat();
		}
	}

	void CameraRotatorComponent::Serialize(JsonWriter& writer) const {
		writer.Key("pitchDegrees");
		writer.Write(mCurrentPitch);
		writer.Key("yawDegrees");
		writer.Write(mCurrentYaw);
	}

	void CameraRotatorComponent::WriteReplayState(
		nlohmann::json& outState
	) const {
		outState["pitchDegrees"] = mCurrentPitch;
		outState["yawDegrees"]   = mCurrentYaw;
	}

	void CameraRotatorComponent::ReadReplayState(
		const nlohmann::json& inState
	) {
		if (!inState.is_object()) {
			return;
		}
		mCurrentPitch = inState.value("pitchDegrees", mCurrentPitch);
		mCurrentYaw   = inState.value("yawDegrees", mCurrentYaw);
		ApplyRotationFromCurrentAngles();
	}

	uint64_t CameraRotatorComponent::ComputeReplayStateHash() const {
		uint64_t hash = ReplayHash::Begin();
		ReplayHash::AppendFloating(hash, mCurrentPitch);
		ReplayHash::AppendFloating(hash, mCurrentYaw);
		return hash;
	}

#ifdef _DEBUG
	void CameraRotatorComponent::DrawInspectorImGui() {
		ImGui::DragFloat("Pitch", &mCurrentPitch, 0.1f, -89.0f, 89.0f);
		ImGui::DragFloat("Yaw", &mCurrentYaw, 0.1f, -1080.0f, 1080.0f);
	}
#endif

	void CameraRotatorComponent::SetLookAnglesDegrees(
		const float pitch, const float yaw
	) {
		mCurrentPitch = pitch;
		mCurrentYaw   = yaw;
	}

	Vec2 CameraRotatorComponent::GetLookAnglesDegrees() const {
		return {mCurrentPitch, mCurrentYaw};
	}

	BaseComponent::TICK_GROUP CameraRotatorComponent::GetTickGroup() const {
		return TICK_GROUP::EARLY;
	}

	TransformComponent* CameraRotatorComponent::GetTransform() const {
		Entity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

	void CameraRotatorComponent::BindLookAxisOnce() const {
		static bool sBound = false;
		if (sBound || !mInput) {
			return;
		}

		constexpr InputKey mouseX = {
			.device = InputDeviceType::MOUSE,
			.code   = VM_X
		};
		constexpr InputKey mouseY = {
			.device = InputDeviceType::MOUSE,
			.code   = VM_Y
		};
		constexpr InputKey gamepadLookX = {
			.device = InputDeviceType::GAMEPAD,
			.code   = VG_RX
		};
		constexpr InputKey gamepadLookY = {
			.device = InputDeviceType::GAMEPAD,
			.code   = VG_RY
		};

		mInput->BindAxis2D("Mouse", mouseX, INPUT_AXIS::X, 1.0f);
		mInput->BindAxis2D("Mouse", mouseY, INPUT_AXIS::Y, 1.0f);
		mInput->BindAxis2D("GamepadLook", gamepadLookX, INPUT_AXIS::X, 1.0f);
		mInput->BindAxis2D("GamepadLook", gamepadLookY, INPUT_AXIS::Y, -1.0f);
		sBound = true;
	}

	void CameraRotatorComponent::ApplyRotationFromCurrentAngles() const {
		auto* transform = GetTransform();
		if (!transform) {
			return;
		}

		const float pitch = mCurrentPitch + mExternalLookOffsetDegrees.x;
		const float yaw   = mCurrentYaw + mExternalLookOffsetDegrees.y;
		transform->SetRotation(
			Quaternion::AxisAngle(Vec3::up, yaw * Math::deg2Rad) *
			Quaternion::AxisAngle(Vec3::right, pitch * Math::deg2Rad)
		);
	}

	REGISTER_COMPONENT(CameraRotatorComponent);
}
