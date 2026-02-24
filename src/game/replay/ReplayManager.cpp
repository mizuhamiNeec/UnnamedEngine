#include "ReplayManager.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>

#include <engine/OldConsole/ConCommand.h>
#include <engine/OldConsole/Console.h>
#include <thirdparty/nlohmann/json.hpp>

namespace {
	constexpr uint32_t kReplayVersion = 2;
	constexpr uint32_t kDefaultTickRate = 66;

	float ReadFloatOrDefault(
		const nlohmann::json& source,
		const char*           key,
		float                 fallback
	) {
		if (!source.contains(key) || !source[key].is_number()) {
			return fallback;
		}
		return source[key].get<float>();
	}

	uint32_t ParseTickRateOrDefault(
		const std::string& rawValue,
		const uint32_t     fallback
	) {
		try {
			const unsigned long parsed = std::stoul(rawValue);
			if (parsed == 0 || parsed > 1000UL) { return fallback; }
			return static_cast<uint32_t>(parsed);
		} catch (const std::exception&) { return fallback; }
	}

	uint32_t ParseTickRateOrDefault(
		const nlohmann::json& source,
		const uint32_t        fallback
	) {
		if (!source.is_number_unsigned()) { return fallback; }
		const uint32_t parsed = source.get<uint32_t>();
		return parsed > 0u ? parsed : fallback;
	}

	uint32_t ReadButtonsOrDefault(const nlohmann::json& source) {
		if (!source.contains("buttons") || !source["buttons"].is_number_unsigned()) {
			return ReplayButton_None;
		}
		return source["buttons"].get<uint32_t>();
	}
}

ReplayManager& ReplayManager::Get() {
	static ReplayManager instance;
	return instance;
}

void ReplayManager::Initialize() {
	if (mInitialized) { return; }
	RegisterConsoleCommands();
	mInitialized = true;
}

bool ReplayManager::LoadClipFromFile(
	const std::string& path, ReplayClip& outClip
) const {
	std::ifstream stream(path);
	if (!stream.is_open()) { return false; }

	nlohmann::json root;
	try {
		stream >> root;
	} catch (const std::exception&) { return false; }

	if (!root.is_object()) { return false; }
	if (!root.contains("version") || !root["version"].is_number_unsigned()) {
		return false;
	}
	if (!root.contains("tickRate") || !root["tickRate"].is_number_unsigned()) {
		return false;
	}
	if (!root.contains("frames") || !root["frames"].is_array()) { return false; }

	outClip.version  = root["version"].get<uint32_t>();
	outClip.tickRate = ParseTickRateOrDefault(root["tickRate"], kDefaultTickRate);
	outClip.frames.clear();
	outClip.frames.reserve(root["frames"].size());

	for (const auto& jsonFrame : root["frames"]) {
		if (!jsonFrame.is_object()) { return false; }

		ReplayUserCmdFrame frame;
		frame.moveX        = ReadFloatOrDefault(jsonFrame, "moveX", 0.0f);
		frame.moveY        = ReadFloatOrDefault(jsonFrame, "moveY", 0.0f);
		frame.buttons      = ReadButtonsOrDefault(jsonFrame);
		frame.viewYawDeg   = ReadFloatOrDefault(jsonFrame, "viewYawDeg", 0.0f);
		frame.viewPitchDeg = ReadFloatOrDefault(jsonFrame, "viewPitchDeg", 0.0f);
		if (jsonFrame.contains("hasAuthoritativeState") &&
		    jsonFrame["hasAuthoritativeState"].is_boolean()) {
			frame.hasAuthoritativeState =
				jsonFrame["hasAuthoritativeState"].get<bool>();
		}
		if (frame.hasAuthoritativeState) {
			frame.playerPosX = ReadFloatOrDefault(jsonFrame, "playerPosX", 0.0f);
			frame.playerPosY = ReadFloatOrDefault(jsonFrame, "playerPosY", 0.0f);
			frame.playerPosZ = ReadFloatOrDefault(jsonFrame, "playerPosZ", 0.0f);
			frame.playerVelX = ReadFloatOrDefault(jsonFrame, "playerVelX", 0.0f);
			frame.playerVelY = ReadFloatOrDefault(jsonFrame, "playerVelY", 0.0f);
			frame.playerVelZ = ReadFloatOrDefault(jsonFrame, "playerVelZ", 0.0f);
		}
		if (jsonFrame.contains("hasVaultState") &&
		    jsonFrame["hasVaultState"].is_boolean()) {
			frame.hasVaultState = jsonFrame["hasVaultState"].get<bool>();
		}
		if (frame.hasVaultState) {
			if (jsonFrame.contains("isSpeedVaulting") &&
			    jsonFrame["isSpeedVaulting"].is_boolean()) {
				frame.isSpeedVaulting = jsonFrame["isSpeedVaulting"].get<bool>();
			}
			frame.vaultProgress = ReadFloatOrDefault(jsonFrame, "vaultProgress", 0.0f);
			frame.vaultStartX = ReadFloatOrDefault(jsonFrame, "vaultStartX", 0.0f);
			frame.vaultStartY = ReadFloatOrDefault(jsonFrame, "vaultStartY", 0.0f);
			frame.vaultStartZ = ReadFloatOrDefault(jsonFrame, "vaultStartZ", 0.0f);
			frame.vaultApexX  = ReadFloatOrDefault(jsonFrame, "vaultApexX", 0.0f);
			frame.vaultApexY  = ReadFloatOrDefault(jsonFrame, "vaultApexY", 0.0f);
			frame.vaultApexZ  = ReadFloatOrDefault(jsonFrame, "vaultApexZ", 0.0f);
			frame.vaultEndX   = ReadFloatOrDefault(jsonFrame, "vaultEndX", 0.0f);
			frame.vaultEndY   = ReadFloatOrDefault(jsonFrame, "vaultEndY", 0.0f);
			frame.vaultEndZ   = ReadFloatOrDefault(jsonFrame, "vaultEndZ", 0.0f);
		}
		outClip.frames.push_back(frame);
	}

	if (outClip.frames.empty()) { outClip.frames.push_back(BuildIdleInputState()); }
	if (outClip.version != kReplayVersion) { outClip.version = kReplayVersion; }
	return true;
}

bool ReplayManager::SaveClipToFile(
	const std::string& path, const ReplayClip& clip
) const {
	const std::filesystem::path outputPath(path);
	if (outputPath.has_parent_path()) {
		std::error_code ec;
		std::filesystem::create_directories(outputPath.parent_path(), ec);
	}

	nlohmann::json root;
	root["version"]  = kReplayVersion;
	root["tickRate"] = clip.tickRate > 0u ? clip.tickRate : kDefaultTickRate;
	root["frames"]   = nlohmann::json::array();

	for (const ReplayUserCmdFrame& frame : clip.frames) {
		nlohmann::json jsonFrame;
		jsonFrame["moveX"]        = frame.moveX;
		jsonFrame["moveY"]        = frame.moveY;
		jsonFrame["buttons"]      = frame.buttons;
		jsonFrame["viewYawDeg"]   = frame.viewYawDeg;
		jsonFrame["viewPitchDeg"] = frame.viewPitchDeg;
		jsonFrame["hasAuthoritativeState"] = frame.hasAuthoritativeState;
		if (frame.hasAuthoritativeState) {
			jsonFrame["playerPosX"] = frame.playerPosX;
			jsonFrame["playerPosY"] = frame.playerPosY;
			jsonFrame["playerPosZ"] = frame.playerPosZ;
			jsonFrame["playerVelX"] = frame.playerVelX;
			jsonFrame["playerVelY"] = frame.playerVelY;
			jsonFrame["playerVelZ"] = frame.playerVelZ;
		}
		jsonFrame["hasVaultState"] = frame.hasVaultState;
		if (frame.hasVaultState) {
			jsonFrame["isSpeedVaulting"] = frame.isSpeedVaulting;
			jsonFrame["vaultProgress"]   = frame.vaultProgress;
			jsonFrame["vaultStartX"]     = frame.vaultStartX;
			jsonFrame["vaultStartY"]     = frame.vaultStartY;
			jsonFrame["vaultStartZ"]     = frame.vaultStartZ;
			jsonFrame["vaultApexX"]      = frame.vaultApexX;
			jsonFrame["vaultApexY"]      = frame.vaultApexY;
			jsonFrame["vaultApexZ"]      = frame.vaultApexZ;
			jsonFrame["vaultEndX"]       = frame.vaultEndX;
			jsonFrame["vaultEndY"]       = frame.vaultEndY;
			jsonFrame["vaultEndZ"]       = frame.vaultEndZ;
		}
		root["frames"].push_back(std::move(jsonFrame));
	}

	std::ofstream stream(path, std::ios::trunc);
	if (!stream.is_open()) { return false; }

	stream << root.dump(2);
	return stream.good();
}

void ReplayManager::StartPlayback(ReplayClip clip, const bool loop) {
	if (clip.frames.empty()) { clip.frames.push_back(BuildIdleInputState()); }
	if (clip.tickRate == 0u) { clip.tickRate = kDefaultTickRate; }
	if (clip.version == 0u) { clip.version = kReplayVersion; }

	mPlaybackClip    = std::move(clip);
	mPlaybackCursor  = 0;
	mPlaybackLoop    = loop;
	mPlaybackActive  = true;
}

void ReplayManager::StopPlayback() {
	mPlaybackActive = false;
	mPlaybackCursor = 0;
	mPlaybackClip   = ReplayClip{};
}

bool ReplayManager::IsPlaybackActive() const { return mPlaybackActive; }

bool ReplayManager::ConsumePlaybackTick(
	ReplayUserCmdFrame& outFrame, bool* outDidLoop
) {
	if (outDidLoop) { *outDidLoop = false; }

	if (!mPlaybackActive || mPlaybackClip.frames.empty()) {
		outFrame = BuildIdleInputState();
		return false;
	}

	if (mPlaybackCursor >= mPlaybackClip.frames.size()) {
		if (mPlaybackLoop) {
			mPlaybackCursor = 0;
			if (outDidLoop) { *outDidLoop = true; }
		} else {
			mPlaybackActive = false;
			outFrame        = BuildIdleInputState();
			return false;
		}
	}

	outFrame = mPlaybackClip.frames[mPlaybackCursor];
	++mPlaybackCursor;

	if (!mPlaybackLoop && mPlaybackCursor >= mPlaybackClip.frames.size()) {
		mPlaybackActive = false;
	}

	return true;
}

uint32_t ReplayManager::GetTickRateOrDefault(const uint32_t fallbackTickRate) const {
	if (mPlaybackClip.tickRate > 0u) { return mPlaybackClip.tickRate; }
	return fallbackTickRate;
}

ReplayManager::ReplayClip ReplayManager::BuildDefaultTitleDemoClip() const {
	ReplayClip clip;
	clip.version  = kReplayVersion;
	clip.tickRate = kDefaultTickRate;
	clip.frames.reserve(static_cast<size_t>(clip.tickRate) * 12u);

	const float tickRateF = static_cast<float>(clip.tickRate);
	for (uint32_t tick = 0; tick < clip.tickRate * 12u; ++tick) {
		const float t = static_cast<float>(tick) / tickRateF;

		ReplayUserCmdFrame frame;
		frame.moveX        = std::clamp(0.45f * std::sin(t * 1.25f), -1.0f, 1.0f);
		frame.moveY        = 1.0f;
		frame.viewYawDeg   = 10.0f + 12.0f * std::sin(t * 0.35f);
		frame.viewPitchDeg = -4.0f + 1.5f * std::sin(t * 0.6f);

		if (tick == clip.tickRate * 2u || tick == clip.tickRate * 5u) {
			frame.buttons |= ReplayButton_Jump;
		}
		if (tick == clip.tickRate * 8u) {
			frame.buttons |= ReplayButton_Blink;
		}
		if (tick >= clip.tickRate * 9u && tick < clip.tickRate * 10u) {
			frame.buttons |= ReplayButton_Crouch;
		}

		clip.frames.push_back(frame);
	}

	if (clip.frames.empty()) { clip.frames.push_back(BuildIdleInputState()); }
	return clip;
}

void ReplayManager::StartRecording(
	const std::string& outputPath, const uint32_t tickRate
) {
	mRecordingPath = outputPath.empty() ?
		                 std::string(kDefaultTitleDemoPath) :
		                 outputPath;
	mRecordingClip = ReplayClip{};
	mRecordingClip.version  = kReplayVersion;
	mRecordingClip.tickRate = tickRate > 0u ? tickRate : kDefaultTickRate;
	mRecordingActive        = true;
}

bool ReplayManager::StopRecordingAndSave() {
	if (!mRecordingActive) { return false; }

	mRecordingActive = false;
	if (mRecordingClip.frames.empty()) {
		mRecordingClip.frames.push_back(BuildIdleInputState());
	}
	return SaveClipToFile(mRecordingPath, mRecordingClip);
}

bool ReplayManager::IsRecording() const { return mRecordingActive; }

uint32_t ReplayManager::GetRecordingTickRateOrDefault(
	const uint32_t fallbackTickRate
) const {
	if (mRecordingClip.tickRate > 0u) { return mRecordingClip.tickRate; }
	return fallbackTickRate;
}

void ReplayManager::CaptureRecordingTick(const ReplayUserCmdFrame& frame) {
	if (!mRecordingActive) { return; }
	mRecordingClip.frames.push_back(frame);
}

ReplayUserCmdFrame ReplayManager::BuildIdleInputState() {
	return {};
}

void ReplayManager::RegisterConsoleCommands() {
	ConCommand::RegisterCommand(
		"demo_record_start",
		[](const std::vector<std::string>& args) {
			std::string path     = std::string(ReplayManager::kDefaultTitleDemoPath);
			uint32_t    tickRate = kDefaultTickRate;

			if (!args.empty()) { path = args[0]; }
			if (args.size() >= 2) {
				tickRate = ParseTickRateOrDefault(args[1], kDefaultTickRate);
			}

			ReplayManager::Get().StartRecording(path, tickRate);
			Console::Print("Demo recording started: " + path);
		},
		"Start demo recording. Usage: demo_record_start [path] [tickrate]"
	);

	ConCommand::RegisterCommand(
		"demo_record_stop",
		[]([[maybe_unused]] const std::vector<std::string>& args) {
			const bool saved = ReplayManager::Get().StopRecordingAndSave();
			Console::Print(
				saved ?
					"Demo recording stopped and saved." :
					"Demo recording was not active."
			);
		},
		"Stop demo recording and save to file."
	);

	ConCommand::RegisterCommand(
		"demo_record_status",
		[]([[maybe_unused]] const std::vector<std::string>& args) {
			const ReplayManager& manager = ReplayManager::Get();
			Console::Print(
				std::string("demo_record_status: ") +
				(manager.IsRecording() ? "recording" : "idle")
			);
			Console::Print(
				"frames: " + std::to_string(manager.mRecordingClip.frames.size())
			);
			Console::Print(
				"tickrate: " + std::to_string(manager.mRecordingClip.tickRate)
			);
			Console::Print("path: " + manager.mRecordingPath);
		},
		"Show demo recording status."
	);
}
