#include "PlayerInputController.h"

#include <engine/Input/InputSystem.h>
#include <game/replay/ReplayManager.h>

namespace {
	bool IsKeyDownNow(const int vk) {
		return (GetAsyncKeyState(vk) & 0x8000) != 0;
	}
}

PlayerInputFrame HumanPlayerInputController::SampleInput() const {
	PlayerInputFrame frame;

	const bool forward =
		InputSystem::IsPressed("forward") || IsKeyDownNow('W') || IsKeyDownNow(VK_UP);
	const bool back =
		InputSystem::IsPressed("back") || IsKeyDownNow('S') || IsKeyDownNow(VK_DOWN);
	const bool right =
		InputSystem::IsPressed("moveright") || IsKeyDownNow('D') || IsKeyDownNow(VK_RIGHT);
	const bool left =
		InputSystem::IsPressed("moveleft") || IsKeyDownNow('A') || IsKeyDownNow(VK_LEFT);

	if (forward) frame.moveInput.y += 1.0f;
	if (back) frame.moveInput.y -= 1.0f;
	if (right) frame.moveInput.x += 1.0f;
	if (left) frame.moveInput.x -= 1.0f;

	frame.moveInput += InputSystem::GetLeftStick(0);
	frame.wishJump   = InputSystem::IsPressed("jump") || IsKeyDownNow(VK_SPACE);
	frame.wishCrouch =
		InputSystem::IsPressed("duck") || IsKeyDownNow(VK_LCONTROL) ||
		IsKeyDownNow(VK_RCONTROL);
	frame.wishBlink  = InputSystem::IsTriggered("blink");
	return frame;
}

void ReplayPlayerInputController::SetReplayInput(
	const Vec2& moveInput, const uint32_t buttons
) {
	mMoveInput         = moveInput;
	mButtons           = buttons;
	mBlinkConsumedTick = false;
}

PlayerInputFrame ReplayPlayerInputController::SampleInput() const {
	PlayerInputFrame frame;
	frame.moveInput  = mMoveInput;
	frame.wishJump   = (mButtons & ReplayButton_Jump) != 0u;
	frame.wishCrouch = (mButtons & ReplayButton_Crouch) != 0u;

	const bool blinkPressed = (mButtons & ReplayButton_Blink) != 0u;
	frame.wishBlink         = blinkPressed && !mBlinkConsumedTick;
	if (frame.wishBlink) { mBlinkConsumedTick = true; }

	return frame;
}
