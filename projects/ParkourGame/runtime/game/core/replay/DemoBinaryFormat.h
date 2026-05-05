#pragma once

#include <cstdint>
#include <type_traits>

namespace Unnamed {
	constexpr uint32_t MakeFourCC(
		const char a,
		const char b,
		const char c,
		const char d
	) {
		return static_cast<uint32_t>(static_cast<uint8_t>(a)) |
		       (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8u) |
		       (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16u) |
		       (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24u);
	}

	constexpr uint32_t kDemoBinaryMagic   = MakeFourCC('U', 'D', 'E', 'M');
	constexpr uint32_t kDemoBinaryVersion = 2u;

	constexpr uint32_t kDemoChunkHead = MakeFourCC('H', 'E', 'A', 'D');
	constexpr uint32_t kDemoChunkInit = MakeFourCC('I', 'N', 'I', 'T');
	constexpr uint32_t kDemoChunkCmds = MakeFourCC('C', 'M', 'D', 'S');
	constexpr uint32_t kDemoChunkSnap = MakeFourCC('S', 'N', 'A', 'P');

	constexpr uint8_t kDemoMoveFlagJumpPressed   = 0x01u;
	constexpr uint8_t kDemoMoveFlagCrouchPressed = 0x02u;
	constexpr uint8_t kDemoMoveFlagSprintPressed = 0x04u;
	constexpr uint8_t kDemoMoveFlagNoclip        = 0x08u;

	constexpr uint16_t kDemoActionFlagGrapplePressed  = 0x0001u;
	constexpr uint16_t kDemoActionFlagGrappleHeld     = 0x0002u;
	constexpr uint16_t kDemoActionFlagGrappleReleased = 0x0004u;
	constexpr uint16_t kDemoActionFlagReelInHeld      = 0x0008u;
	constexpr uint16_t kDemoActionFlagReelOutHeld     = 0x0010u;
	constexpr uint16_t kDemoActionFlagWeaponPrimaryPressed = 0x0020u;
	constexpr uint16_t kDemoActionFlagWeaponPrimaryHeld = 0x0040u;
	constexpr uint16_t kDemoActionFlagWeaponPrimaryReleased = 0x0080u;
	constexpr uint16_t kDemoActionFlagWeaponSecondaryPressed = 0x0100u;
	constexpr uint16_t kDemoActionFlagWeaponSecondaryHeld = 0x0200u;
	constexpr uint16_t kDemoActionFlagWeaponSecondaryReleased = 0x0400u;
	constexpr uint16_t kDemoActionFlagWeaponReloadPressed = 0x0800u;
	constexpr uint16_t kDemoActionFlagWeaponCycleNextPressed = 0x1000u;
	constexpr uint16_t kDemoActionFlagWeaponCyclePrevPressed = 0x2000u;

	struct DemoBinaryFileHeader {
		uint32_t magic      = kDemoBinaryMagic;
		uint32_t version    = kDemoBinaryVersion;
		uint32_t chunkCount = 0;
		uint32_t reserved   = 0;
	};

	struct DemoBinaryChunkHeader {
		uint32_t id   = 0;
		uint32_t size = 0;
	};

	struct DemoBinaryPackedPlayerCommand {
		uint64_t tick              = 0;
		uint64_t subjectEntityGuid = 0;
		uint8_t  commandType       = 0;
		uint8_t  flags             = 0;
		uint16_t actionFlags       = 0;
		float    moveAxis[3]       = {};
		float    right[3]          = {};
		float    up[3]             = {};
		float    forward[3]        = {};
		float    viewYawDeg        = 0.0f;
		float    viewPitchDeg      = 0.0f;
	};

	static_assert(std::is_trivially_copyable_v<DemoBinaryFileHeader>);
	static_assert(std::is_trivially_copyable_v<DemoBinaryChunkHeader>);
	static_assert(std::is_trivially_copyable_v<DemoBinaryPackedPlayerCommand>);
}
