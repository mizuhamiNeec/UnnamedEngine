#include "CameraRotator.h"

#include "Camera/CameraManager.h"
#include "Components/CameraComponent.h"
#include "Components/TransformComponent.h"
#include "Entity/Base/Entity.h"
#include "Input/InputSystem.h"
#include "Lib/Console/ConCommand.h"
#include "Lib/Console/ConVarManager.h"
#include "Lib/Math/Vector/Vec3.h"
#include "Lib/Timer/EngineTimer.h"

CameraRotator::~CameraRotator() {}

void CameraRotator::OnAttach(Entity& owner) {
	Component::OnAttach(owner);

	transform_ = owner_->GetTransform();
	ConCommand::RegisterCommand(
		"togglelockcursor",
		[this]([[maybe_unused]] const std::vector<std::string>& args) {
			isMouseLocked_ = !isMouseLocked_;
		},
		"Toggle lock cursor."
	);
}

void CameraRotator::Update([[maybe_unused]] float deltaTime) {
	static bool cursorHidden = false;
	if (isMouseLocked_) {
		if (!cursorHidden) {
			ShowCursor(FALSE); // カーソルを非表示にする
			cursorHidden = true;
		}
		// カーソルをウィンドウの中央にリセット
		POINT centerCursorPos = {
			static_cast<LONG>(Window::GetClientWidth() / 2), static_cast<LONG>(Window::GetClientHeight() / 2)
		};
		ClientToScreen(Window::GetWindowHandle(), &centerCursorPos); // クライアント座標をスクリーン座標に変換
		SetCursorPos(centerCursorPos.x, centerCursorPos.y);
	} else {
		if (cursorHidden) {
			ShowCursor(TRUE); // カーソルを表示する
			cursorHidden = false;
		}
	}

	Vec2 delta = InputSystem::GetMouseDelta();
	// 回転
	float sensitivity = ConVarManager::GetConVar("sensitivity")->GetValueAsFloat();
	float m_pitch = 0.022f;
	float m_yaw = 0.022f;
	float min = -89.0f;
	float max = 89.0f;

	rot_.y += delta.y * sensitivity * m_pitch * Math::deg2Rad;
	rot_.x += delta.x * sensitivity * m_yaw * Math::deg2Rad;

	rot_.y = std::clamp(rot_.y, min * Math::deg2Rad, max * Math::deg2Rad);

	transform_->SetWorldRot(Quaternion::Euler(Vec3::up * rot_.x + Vec3::right * rot_.y));
}

void CameraRotator::DrawInspectorImGui() {}
