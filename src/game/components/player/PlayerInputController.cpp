#include "PlayerInputController.h"

#include <engine/Input/InputSystem.h>
#include <game/replay/ReplayManager.h>

PlayerInputFrame HumanPlayerInputController::SampleInput() const {
	PlayerInputFrame frame;

	if (InputSystem::IsPressed("forward")) frame.moveInput.y += 1.0f;
	if (InputSystem::IsPressed("back")) frame.moveInput.y -= 1.0f;
	if (InputSystem::IsPressed("moveright")) frame.moveInput.x += 1.0f;
	if (InputSystem::IsPressed("moveleft")) frame.moveInput.x -= 1.0f;

	frame.moveInput += InputSystem::GetLeftStick(0);
	frame.wishJump   = InputSystem::IsPressed("jump");
	frame.wishCrouch = InputSystem::IsPressed("duck");
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
