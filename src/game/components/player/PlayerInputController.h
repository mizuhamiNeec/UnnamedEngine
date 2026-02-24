#pragma once

#include <cstdint>

#include <runtime/core/math/Math.h>

struct PlayerInputFrame {
	Vec2 moveInput  = Vec2::zero;
	bool wishJump   = false;
	bool wishCrouch = false;
	bool wishBlink  = false;
};

class IPlayerInputController {
public:
	virtual ~IPlayerInputController() = default;

	virtual PlayerInputFrame SampleInput() const = 0;
};

class HumanPlayerInputController final : public IPlayerInputController {
public:
	PlayerInputFrame SampleInput() const override;
};

class ReplayPlayerInputController final : public IPlayerInputController {
public:
	void SetReplayInput(const Vec2& moveInput, uint32_t buttons);
	PlayerInputFrame SampleInput() const override;

private:
	Vec2             mMoveInput         = Vec2::zero;
	uint32_t         mButtons           = 0u;
	mutable bool     mBlinkConsumedTick = false;
};
