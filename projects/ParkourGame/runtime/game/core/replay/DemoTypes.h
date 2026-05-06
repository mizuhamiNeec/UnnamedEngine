#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <json.hpp>

#include "DemoBinaryFormat.h"
#include "game/core/components/character/base/BaseCharacterComponent.h"
#include "game/core/input/CharacterActionFrameInput.h"

namespace Unnamed {
	constexpr uint32_t kDemoFileVersion = kDemoBinaryVersion;
	constexpr uint32_t kDefaultDemoTickRate = 66; // 1秒あたりのティック数

	enum class DEMO_COMMAND_TYPE : uint8_t {
		PLAYER_INPUT = 0,
	};

	struct DemoPlayerInputPayload {
		MovementFrameInput movement     = {};
		CharacterActionFrameInput action = {};
		float              viewYawDeg   = 0.0f;
		float              viewPitchDeg = 0.0f;
	};

	struct DemoTickCommand {
		uint64_t               tick = 0;
		uint64_t               subjectEntityGuid = 0;
		DEMO_COMMAND_TYPE      commandType = DEMO_COMMAND_TYPE::PLAYER_INPUT;
		DemoPlayerInputPayload playerInput = {};
	};

	struct EntitySnapshotRecord {
		uint64_t       entityGuid = 0;
		std::string    entityType;
		uint32_t       entityTypeId = 0;
		nlohmann::json state        = nlohmann::json::object();
		uint64_t       hash         = 0;
	};

	struct FrameSnapshot {
		uint64_t                          tick     = 0;
		std::vector<EntitySnapshotRecord> entities = {};
	};

	struct DemoFileV2 {
		uint32_t                                     version = kDemoFileVersion;
		uint32_t                                     tickRate = kDefaultDemoTickRate;
		uint64_t                                     startTick = 0;
		std::string                                  mapPath;
		std::unordered_map<std::string, std::string> meta            = {};
		std::vector<EntitySnapshotRecord>            initialEntities = {};
		std::vector<DemoTickCommand>                 commands        = {};
		std::vector<FrameSnapshot>                   snapshots       = {};
	};
}
