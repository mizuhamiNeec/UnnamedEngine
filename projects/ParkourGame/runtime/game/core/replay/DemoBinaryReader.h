#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include <json.hpp>


#include "DemoBinaryFormat.h"
#include "DemoTypes.h"

#include "core/io/binary/BinaryReader.h"

namespace Unnamed {
	class DemoBinaryReader {
	public:
		[[nodiscard]] bool ReadFile(
			const std::string& path,
			DemoFileV2&        outFile,
			std::string*       outError = nullptr
		) const {
			BinaryReader reader(path);
			if (!reader.IsOpen()) {
				SetError(outError, "Failed to open demo file.");
				return false;
			}

			DemoBinaryFileHeader header = {};
			if (!reader.ReadPod(header)) {
				SetError(outError, "Failed to read demo header.");
				return false;
			}
			if (header.magic != kDemoBinaryMagic) {
				SetError(outError, "Invalid demo magic.");
				return false;
			}
			if (header.version != kDemoBinaryVersion) {
				SetError(outError, "Unsupported demo binary version.");
				return false;
			}

			outFile         = {};
			outFile.version = header.version;

			bool sawHead = false;
			for (uint32_t i = 0; i < header.chunkCount; ++i) {
				DemoBinaryChunkHeader chunk = {};
				if (!reader.ReadPod(chunk)) {
					SetError(outError, "Failed to read chunk header.");
					return false;
				}
				if (reader.Remaining() < chunk.size) {
					SetError(outError, "Truncated chunk payload.");
					return false;
				}

				const uint64_t chunkStart = reader.Tell();
				const uint64_t chunkEnd   = chunkStart + chunk.size;
				bool           parseOk;
				switch (chunk.id) {
					case kDemoChunkHead: parseOk = ReadHeadChunk(
						                     reader, outFile
					                     );
						sawHead = parseOk;
						break;
					case kDemoChunkInit: parseOk = ReadInitChunk(
						                     reader, outFile
					                     );
						break;
					case kDemoChunkCmds: parseOk = ReadCommandsChunk(
						                     reader, outFile
					                     );
						break;
					case kDemoChunkSnap: parseOk = ReadSnapChunk(
						                     reader, outFile
					                     );
						break;
					default: parseOk = reader.Skip(chunk.size);
						break;
				}

				if (!parseOk) {
					SetError(outError, "Failed to parse demo chunk.");
					return false;
				}

				const uint64_t cursor = reader.Tell();
				if (cursor < chunkEnd) {
					if (!reader.Seek(chunkEnd)) {
						SetError(outError, "Failed to seek after chunk.");
						return false;
					}
				} else if (cursor > chunkEnd) {
					SetError(outError, "Chunk over-read detected.");
					return false;
				}
			}

			if (!sawHead) {
				SetError(outError, "Missing HEAD chunk.");
				return false;
			}

			SortLoadedData(outFile);
			return true;
		}

	private:
		static void SetError(
			std::string* outError, const std::string_view text
		) {
			if (outError) {
				*outError = std::string(text);
			}
		}

		[[nodiscard]] static bool ReadHeadChunk(
			BinaryReader& reader, DemoFileV2& outFile
		) {
			if (!reader.ReadPod(outFile.tickRate) ||
			    !reader.ReadPod(outFile.startTick) ||
			    !reader.ReadString(outFile.mapPath)) {
				return false;
			}

			uint32_t metaCount = 0;
			if (!reader.ReadPod(metaCount)) {
				return false;
			}

			outFile.meta.clear();
			for (uint32_t i = 0; i < metaCount; ++i) {
				std::string key;
				std::string value;
				if (!reader.ReadString(key) || !reader.ReadString(value)) {
					return false;
				}
				outFile.meta[key] = value;
			}
			return true;
		}

		[[nodiscard]] static bool ReadInitChunk(
			BinaryReader& reader, DemoFileV2& outFile
		) {
			uint32_t count = 0;
			if (!reader.ReadPod(count)) {
				return false;
			}

			outFile.initialEntities.clear();
			outFile.initialEntities.reserve(count);
			for (uint32_t i = 0; i < count; ++i) {
				EntitySnapshotRecord record = {};
				if (!reader.ReadPod(record.entityGuid) ||
				    !reader.ReadString(record.entityType) ||
				    !reader.ReadPod(record.hash)) {
					return false;
				}

				std::string stateText;
				if (!reader.ReadString(stateText)) {
					return false;
				}
				if (stateText.empty()) {
					record.state = nlohmann::json::object();
				} else {
					try {
						record.state = nlohmann::json::parse(stateText);
					} catch (...) {
						return false;
					}
				}

				outFile.initialEntities.emplace_back(std::move(record));
			}

			return true;
		}

		[[nodiscard]] static bool ReadCommandsChunk(
			BinaryReader& reader, DemoFileV2& outFile
		) {
			uint32_t count = 0;
			if (!reader.ReadPod(count)) {
				return false;
			}

			outFile.commands.clear();
			outFile.commands.reserve(count);
			for (uint32_t i = 0; i < count; ++i) {
				DemoBinaryPackedPlayerCommand packed = {};
				if (!reader.ReadPod(packed)) {
					return false;
				}

				DemoTickCommand command   = {};
				command.tick              = packed.tick;
				command.subjectEntityGuid = packed.subjectEntityGuid;
				command.commandType       = static_cast<DEMO_COMMAND_TYPE>(
					packed.commandType
				);
				if (command.commandType != DEMO_COMMAND_TYPE::PLAYER_INPUT) {
					return false;
				}
				command.playerInput.movement.moveAxis = Vec3(
					packed.moveAxis[0],
					packed.moveAxis[1],
					packed.moveAxis[2]
				);
				command.playerInput.movement.right = Vec3(
					packed.right[0],
					packed.right[1],
					packed.right[2]
				);
				command.playerInput.movement.up = Vec3(
					packed.up[0],
					packed.up[1],
					packed.up[2]
				);
				command.playerInput.movement.forward = Vec3(
					packed.forward[0],
					packed.forward[1],
					packed.forward[2]
				);
				command.playerInput.movement.jumpPressed =
					(packed.flags & kDemoMoveFlagJumpPressed) != 0u;
				command.playerInput.movement.crouchPressed =
					(packed.flags & kDemoMoveFlagCrouchPressed) != 0u;
				command.playerInput.movement.sprintPressed =
					(packed.flags & kDemoMoveFlagSprintPressed) != 0u;
				command.playerInput.movement.noclip =
					(packed.flags & kDemoMoveFlagNoclip) != 0u;
				command.playerInput.action.weapon.primary.pressed =
				(packed.actionFlags &
				 kDemoActionFlagWeaponPrimaryPressed) != 0u;
				command.playerInput.action.weapon.primary.held =
				(packed.actionFlags &
				 kDemoActionFlagWeaponPrimaryHeld) != 0u;
				command.playerInput.action.weapon.primary.released =
				(packed.actionFlags &
				 kDemoActionFlagWeaponPrimaryReleased) != 0u;
				command.playerInput.action.weapon.secondary.pressed =
				(packed.actionFlags &
				 kDemoActionFlagWeaponSecondaryPressed) != 0u;
				command.playerInput.action.weapon.secondary.held =
				(packed.actionFlags &
				 kDemoActionFlagWeaponSecondaryHeld) != 0u;
				command.playerInput.action.weapon.secondary.released =
				(packed.actionFlags &
				 kDemoActionFlagWeaponSecondaryReleased) != 0u;
				command.playerInput.action.weapon.reloadPressed =
				(packed.actionFlags &
				 kDemoActionFlagWeaponReloadPressed) != 0u;
				command.playerInput.action.weapon.cycleNextPressed =
				(packed.actionFlags &
				 kDemoActionFlagWeaponCycleNextPressed) != 0u;
				command.playerInput.action.weapon.cyclePrevPressed =
				(packed.actionFlags &
				 kDemoActionFlagWeaponCyclePrevPressed) != 0u;
				command.playerInput.viewYawDeg   = packed.viewYawDeg;
				command.playerInput.viewPitchDeg = packed.viewPitchDeg;
				outFile.commands.emplace_back(command);
			}
			return true;
		}

		[[nodiscard]] static bool ReadSnapChunk(
			BinaryReader& reader, DemoFileV2& outFile
		) {
			uint32_t frameCount = 0;
			if (!reader.ReadPod(frameCount)) {
				return false;
			}

			outFile.snapshots.clear();
			outFile.snapshots.reserve(frameCount);
			for (uint32_t i = 0; i < frameCount; ++i) {
				FrameSnapshot frame = {};
				if (!reader.ReadPod(frame.tick)) {
					return false;
				}

				uint32_t entityCount = 0;
				if (!reader.ReadPod(entityCount)) {
					return false;
				}
				frame.entities.reserve(entityCount);
				for (uint32_t j = 0; j < entityCount; ++j) {
					EntitySnapshotRecord record       = {};
					uint32_t             entityTypeId = 0;
					if (!reader.ReadPod(record.entityGuid) ||
					    !reader.ReadPod(entityTypeId) ||
					    !reader.ReadPod(record.hash)) {
						return false;
					}
					record.entityTypeId = entityTypeId;
					record.entityType.clear();
					record.state = nlohmann::json::object();
					frame.entities.emplace_back(std::move(record));
				}
				outFile.snapshots.emplace_back(std::move(frame));
			}
			return true;
		}

		static void SortLoadedData(DemoFileV2& file) {
			std::ranges::sort(
				file.initialEntities,
				[](
				const EntitySnapshotRecord& lhs, const EntitySnapshotRecord& rhs
			) {
					return lhs.entityGuid < rhs.entityGuid;
				}
			);

			std::ranges::sort(
				file.commands,
				[](const DemoTickCommand& lhs, const DemoTickCommand& rhs) {
					if (lhs.tick != rhs.tick) {
						return lhs.tick < rhs.tick;
					}
					if (lhs.subjectEntityGuid != rhs.subjectEntityGuid) {
						return lhs.subjectEntityGuid < rhs.subjectEntityGuid;
					}
					return static_cast<uint32_t>(lhs.commandType) <
					       static_cast<uint32_t>(rhs.commandType);
				}
			);

			std::ranges::sort(
				file.snapshots,
				[](const FrameSnapshot& lhs, const FrameSnapshot& rhs) {
					if (lhs.tick != rhs.tick) {
						return lhs.tick < rhs.tick;
					}
					const uint64_t lhsGuid = lhs.entities.empty() ?
						                         0ull :
						                         lhs.entities.front().
						                         entityGuid;
					const uint64_t rhsGuid = rhs.entities.empty() ?
						                         0ull :
						                         rhs.entities.front().
						                         entityGuid;
					return lhsGuid < rhsGuid;
				}
			);
			for (FrameSnapshot& snapshot : file.snapshots) {
				std::ranges::sort(
					snapshot.entities,
					[](
					const EntitySnapshotRecord& lhs,
					const EntitySnapshotRecord& rhs
				) {
						return lhs.entityGuid < rhs.entityGuid;
					}
				);
			}
		}
	};
}
