#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

enum ReplayButtonFlags : uint32_t {
	ReplayButton_None   = 0u,
	ReplayButton_Jump   = 1u << 0,
	ReplayButton_Crouch = 1u << 1,
	ReplayButton_Blink  = 1u << 2,
	ReplayButton_Attack1 = 1u << 3,
	ReplayButton_Reload  = 1u << 4
};

struct ReplayUserCmdFrame {
	float    moveX        = 0.0f;
	float    moveY        = 0.0f;
	uint32_t buttons      = ReplayButton_None;
	float    viewYawDeg   = 0.0f;
	float    viewPitchDeg = 0.0f;
	bool     hasAuthoritativeState = false;
	float    playerPosX            = 0.0f;
	float    playerPosY            = 0.0f;
	float    playerPosZ            = 0.0f;
	float    playerVelX            = 0.0f;
	float    playerVelY            = 0.0f;
	float    playerVelZ            = 0.0f;
	bool     hasVaultState         = false;
	bool     isSpeedVaulting       = false;
	float    vaultProgress         = 0.0f;
	float    vaultStartX           = 0.0f;
	float    vaultStartY           = 0.0f;
	float    vaultStartZ           = 0.0f;
	float    vaultApexX            = 0.0f;
	float    vaultApexY            = 0.0f;
	float    vaultApexZ            = 0.0f;
	float    vaultEndX             = 0.0f;
	float    vaultEndY             = 0.0f;
	float    vaultEndZ             = 0.0f;
};

class ReplayManager {
public:
	static constexpr std::string_view kDefaultTitleDemoPath =
		"./content/parkour/replay/title_demo.json";

	struct ReplayClip {
		uint32_t version  = 2;
		uint32_t tickRate = 66;
		std::vector<ReplayUserCmdFrame> frames;
	};

	static ReplayManager& Get();

	void Initialize();

	bool LoadClipFromFile(const std::string& path, ReplayClip& outClip) const;
	bool SaveClipToFile(const std::string& path, const ReplayClip& clip) const;

	void StartPlayback(ReplayClip clip, bool loop);
	void StopPlayback();
	bool IsPlaybackActive() const;
	bool ConsumePlaybackTick(
		ReplayUserCmdFrame& outFrame, bool* outDidLoop = nullptr
	);
	uint32_t GetTickRateOrDefault(uint32_t fallbackTickRate) const;

	ReplayClip BuildDefaultTitleDemoClip() const;

	void StartRecording(const std::string& outputPath, uint32_t tickRate);
	bool StopRecordingAndSave();
	bool IsRecording() const;
	uint32_t GetRecordingTickRateOrDefault(uint32_t fallbackTickRate) const;
	void CaptureRecordingTick(const ReplayUserCmdFrame& frame);

private:
	ReplayManager() = default;

	static ReplayUserCmdFrame BuildIdleInputState();

	void RegisterConsoleCommands();

	bool      mInitialized  = false;
	ReplayClip mPlaybackClip;
	size_t    mPlaybackCursor = 0;
	bool      mPlaybackLoop   = true;
	bool      mPlaybackActive = false;

	ReplayClip  mRecordingClip;
	bool        mRecordingActive = false;
	std::string mRecordingPath;
};
