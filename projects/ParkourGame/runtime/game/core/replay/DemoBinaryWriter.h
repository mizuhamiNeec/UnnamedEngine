#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <json.hpp>


#include "DemoBinaryFormat.h"
#include "DemoTypes.h"

#include "core/io/binary/BinaryWriter.h"

namespace Unnamed {
	class DemoBinaryWriter {
	public:
		[[nodiscard]] bool WriteFile(
			const std::string& path,
			const DemoFileV2&  file,
			std::string*       outError = nullptr
		) const {
			BinaryWriter writer(path);
			if (!writer.IsOpen()) {
				SetError(outError, "Failed to open file for write.");
				return false;
			}

			DemoBinaryFileHeader header = {};
			header.chunkCount           = 4u;
			if (!writer.WritePod(header)) {
				SetError(outError, "Failed to write demo header.");
				return false;
			}

			if (!WriteHeadChunk(writer, file, outError) ||
			    !WriteInitChunk(writer, file, outError) ||
			    !WriteCommandsChunk(writer, file, outError) ||
			    !WriteSnapChunk(writer, file, outError)) {
				return false;
			}

			if (!writer.Flush()) {
				SetError(outError, "Failed to flush demo file.");
				return false;
			}
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

		template <typename Fn>
		[[nodiscard]] static bool WriteChunk(
			BinaryWriter&           writer,
			const uint32_t          chunkId,
			Fn&&                    payloadWriter,
			std::string*            outError,
			const std::string_view  chunkName
		) {
			const uint64_t chunkHeaderPos = writer.Tell();
			DemoBinaryChunkHeader chunkHeader = {};
			chunkHeader.id                   = chunkId;
			if (!writer.WritePod(chunkHeader)) {
				SetError(outError, "Failed to write chunk header.");
				return false;
			}

			const uint64_t payloadBegin = writer.Tell();
			if (!payloadWriter()) {
				SetError(
					outError,
					std::string("Failed to write chunk payload: ") +
						std::string(chunkName)
				);
				return false;
			}
			const uint64_t payloadEnd = writer.Tell();
			const uint64_t payloadSize64 = payloadEnd - payloadBegin;
			if (payloadSize64 > static_cast<uint64_t>(UINT32_MAX)) {
				SetError(outError, "Chunk payload exceeded u32 size.");
				return false;
			}

			chunkHeader.size = static_cast<uint32_t>(payloadSize64);
			if (!writer.Seek(chunkHeaderPos) ||
			    !writer.WritePod(chunkHeader) ||
			    !writer.Seek(payloadEnd)) {
				SetError(outError, "Failed to patch chunk size.");
				return false;
			}
			return true;
		}

		[[nodiscard]] static uint32_t EntityTypeId(const std::string_view text) {
			constexpr uint32_t kOffset = 2166136261u;
			uint32_t           hash    = kOffset;
			for (const char ch : text) {
				constexpr uint32_t kPrime = 16777619u;
				hash                      ^= static_cast<uint8_t>(ch);
				hash                      *= kPrime;
			}
			return hash;
		}

		[[nodiscard]] static bool WriteHeadChunk(
			BinaryWriter&     writer,
			const DemoFileV2& file,
			std::string*      outError
		) {
			return WriteChunk(
				writer,
				kDemoChunkHead,
				[&]() {
					if (!writer.WritePod(file.tickRate) ||
					    !writer.WritePod(file.startTick) ||
					    !writer.WriteString(file.mapPath)) {
						return false;
					}

					std::vector<std::pair<std::string, std::string>> metaEntries(
						file.meta.begin(),
						file.meta.end()
					);
					std::ranges::sort(
						metaEntries,
						[](const auto& lhs, const auto& rhs) {
							return lhs.first < rhs.first;
						}
					);

					const uint32_t metaCount = static_cast<uint32_t>(
						metaEntries.size()
					);
					if (!writer.WritePod(metaCount)) {
						return false;
					}

					for (const auto& [key, value] : metaEntries) {
						if (!writer.WriteString(key) ||
						    !writer.WriteString(value)) {
							return false;
						}
					}

					return true;
				},
				outError,
				"HEAD"
			);
		}

		[[nodiscard]] static bool WriteInitChunk(
			BinaryWriter&     writer,
			const DemoFileV2& file,
			std::string*      outError
		) {
			return WriteChunk(
				writer,
				kDemoChunkInit,
				[&]() {
					std::vector<EntitySnapshotRecord> initial = file.initialEntities;
					std::ranges::sort(
						initial,
						[](const EntitySnapshotRecord& lhs,
						   const EntitySnapshotRecord& rhs) {
							return lhs.entityGuid < rhs.entityGuid;
						}
					);

					const uint32_t count = static_cast<uint32_t>(initial.size());
					if (!writer.WritePod(count)) {
						return false;
					}

					for (const EntitySnapshotRecord& record : initial) {
						if (!writer.WritePod(record.entityGuid) ||
						    !writer.WriteString(record.entityType) ||
						    !writer.WritePod(record.hash)) {
							return false;
						}

						const std::string stateText = record.state.dump();
						if (!writer.WriteString(stateText)) {
							return false;
						}
					}
					return true;
				},
				outError,
				"INIT"
			);
		}

		[[nodiscard]] static bool WriteCommandsChunk(
			BinaryWriter&     writer,
			const DemoFileV2& file,
			std::string*      outError
		) {
			return WriteChunk(
				writer,
				kDemoChunkCmds,
				[&]() {
					const uint32_t count = static_cast<uint32_t>(file.commands.size());
					if (!writer.WritePod(count)) {
						return false;
					}

					for (const DemoTickCommand& command : file.commands) {
						if (command.commandType != DEMO_COMMAND_TYPE::PLAYER_INPUT) {
							return false;
						}

						DemoBinaryPackedPlayerCommand packed = {};
						packed.tick                          = command.tick;
						packed.subjectEntityGuid             = command.subjectEntityGuid;
						packed.commandType = static_cast<uint8_t>(command.commandType);

						uint8_t flags = 0;
						if (command.playerInput.movement.jumpPressed) {
							flags |= kDemoMoveFlagJumpPressed;
						}
						if (command.playerInput.movement.crouchPressed) {
							flags |= kDemoMoveFlagCrouchPressed;
						}
						if (command.playerInput.movement.sprintPressed) {
							flags |= kDemoMoveFlagSprintPressed;
						}
						if (command.playerInput.movement.noclip) {
							flags |= kDemoMoveFlagNoclip;
						}
						packed.flags = flags;

						uint16_t actionFlags = 0;
						const auto& weapon = command.playerInput.action.weapon;
						if (weapon.primary.pressed) {
							actionFlags |= kDemoActionFlagWeaponPrimaryPressed;
						}
						if (weapon.primary.held) {
							actionFlags |= kDemoActionFlagWeaponPrimaryHeld;
						}
						if (weapon.primary.released) {
							actionFlags |= kDemoActionFlagWeaponPrimaryReleased;
						}
						if (weapon.secondary.pressed) {
							actionFlags |= kDemoActionFlagWeaponSecondaryPressed;
						}
						if (weapon.secondary.held) {
							actionFlags |= kDemoActionFlagWeaponSecondaryHeld;
						}
						if (weapon.secondary.released) {
							actionFlags |=
								kDemoActionFlagWeaponSecondaryReleased;
						}
						if (weapon.reloadPressed) {
							actionFlags |= kDemoActionFlagWeaponReloadPressed;
						}
						if (weapon.cycleNextPressed) {
							actionFlags |=
								kDemoActionFlagWeaponCycleNextPressed;
						}
						if (weapon.cyclePrevPressed) {
							actionFlags |=
								kDemoActionFlagWeaponCyclePrevPressed;
						}
						packed.actionFlags = actionFlags;

						const auto& movement = command.playerInput.movement;
						packed.moveAxis[0]   = movement.moveAxis.x;
						packed.moveAxis[1]   = movement.moveAxis.y;
						packed.moveAxis[2]   = movement.moveAxis.z;
						packed.right[0]      = movement.right.x;
						packed.right[1]      = movement.right.y;
						packed.right[2]      = movement.right.z;
						packed.up[0]         = movement.up.x;
						packed.up[1]         = movement.up.y;
						packed.up[2]         = movement.up.z;
						packed.forward[0]    = movement.forward.x;
						packed.forward[1]    = movement.forward.y;
						packed.forward[2]    = movement.forward.z;
						packed.viewYawDeg    = command.playerInput.viewYawDeg;
						packed.viewPitchDeg  = command.playerInput.viewPitchDeg;
						if (!writer.WritePod(packed)) {
							return false;
						}
					}
					return true;
				},
				outError,
				"CMDS"
			);
		}

		[[nodiscard]] static bool WriteSnapChunk(
			BinaryWriter&     writer,
			const DemoFileV2& file,
			std::string*      outError
		) {
			return WriteChunk(
				writer,
				kDemoChunkSnap,
				[&]() {
					const uint32_t frameCount = static_cast<uint32_t>(
						file.snapshots.size()
					);
					if (!writer.WritePod(frameCount)) {
						return false;
					}

					for (const FrameSnapshot& snapshot : file.snapshots) {
						if (!writer.WritePod(snapshot.tick)) {
							return false;
						}
						const uint32_t entityCount = static_cast<uint32_t>(
							snapshot.entities.size()
						);
						if (!writer.WritePod(entityCount)) {
							return false;
						}
						for (const EntitySnapshotRecord& record : snapshot.entities) {
							const uint32_t entityTypeId = record.entityTypeId != 0u ?
								                              record.entityTypeId :
								                              EntityTypeId(record.entityType);
							if (!writer.WritePod(record.entityGuid) ||
							    !writer.WritePod(entityTypeId) ||
							    !writer.WritePod(record.hash)) {
								return false;
							}
						}
					}
					return true;
				},
				outError,
				"SNAP"
			);
		}
	};
}
