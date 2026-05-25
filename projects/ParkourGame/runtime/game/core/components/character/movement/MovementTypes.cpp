#include "MovementTypes.h"

namespace Unnamed {
	void MovementContext::ResetTransitions() {
		transitionCount = 0;
	}

	bool MovementContext::SubmitTransition(
		const MOVEMENT_MODE_ID mode,
		const MOVEMENT_TRANSITION_PRIORITY priority,
		const std::string_view reason,
		const std::string_view source
	) {
		if (transitionCount >= transitionBuffer.size()) {
			return false;
		}

		transitionBuffer[transitionCount++] = MovementTransitionRequest{
			.toMode   = mode,
			.priority = priority,
			.reason   = reason,
			.source   = source
		};
		return true;
	}

	bool MovementContext::IsCapabilityEnabled(const MOVEMENT_ABILITY_SLOT slot)
	const {
		if (!capabilitySet) {
			return false;
		}

		switch (slot) {
			case MOVEMENT_ABILITY_SLOT::JUMP: return capabilitySet->jump;
			case MOVEMENT_ABILITY_SLOT::CROUCH: return capabilitySet->crouch;
			case MOVEMENT_ABILITY_SLOT::SLIDE: return capabilitySet->slide;
			case MOVEMENT_ABILITY_SLOT::WALL_RUN: return capabilitySet->wallRun;
			case MOVEMENT_ABILITY_SLOT::DOUBLE_JUMP: return capabilitySet->doubleJump;
			case MOVEMENT_ABILITY_SLOT::SPEED_VAULT: return capabilitySet->speedVault;
			case MOVEMENT_ABILITY_SLOT::BLINK: return capabilitySet->blink;
			case MOVEMENT_ABILITY_SLOT::GRAPPLE: return capabilitySet->grapple;
			case MOVEMENT_ABILITY_SLOT::COUNT: return false;
		}

		return false;
	}

	bool MovementContext::IsAbilityActive(const MOVEMENT_ABILITY_SLOT slot) const {
		return (activeAbilityMask & AbilitySlotToMask(slot)) != 0;
	}

	uint64_t AbilitySlotToMask(const MOVEMENT_ABILITY_SLOT slot) {
		return 1ull << static_cast<uint8_t>(slot);
	}

	std::string_view ToString(const MOVEMENT_MODE_ID modeId) {
		switch (modeId) {
			case MOVEMENT_MODE_ID::GROUND: return "GroundMove";
			case MOVEMENT_MODE_ID::AIR: return "AirMove";
			case MOVEMENT_MODE_ID::NOCLIP: return "NoclipMove";
			case MOVEMENT_MODE_ID::COUNT: return "Unknown";
		}

		return "Unknown";
	}

	std::string_view ToString(const MOVEMENT_ABILITY_SLOT slot) {
		switch (slot) {
			case MOVEMENT_ABILITY_SLOT::JUMP: return "jump";
			case MOVEMENT_ABILITY_SLOT::CROUCH: return "crouch";
			case MOVEMENT_ABILITY_SLOT::SLIDE: return "slide";
			case MOVEMENT_ABILITY_SLOT::WALL_RUN: return "wallrun";
			case MOVEMENT_ABILITY_SLOT::DOUBLE_JUMP: return "doublejump";
			case MOVEMENT_ABILITY_SLOT::SPEED_VAULT: return "speedvault";
			case MOVEMENT_ABILITY_SLOT::BLINK: return "blink";
			case MOVEMENT_ABILITY_SLOT::GRAPPLE: return "grapple";
			case MOVEMENT_ABILITY_SLOT::COUNT: return "unknown";
		}

		return "unknown";
	}
}
