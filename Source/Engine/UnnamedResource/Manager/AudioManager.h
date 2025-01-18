#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <wrl.h>
#include <UnnamedResource/Audio.h>

using Microsoft::WRL::ComPtr;

class AudioManager {
public:
	AudioManager();
	~AudioManager();

	bool Init();
	void Shutdown();

	std::shared_ptr<Audio> GetAudio(const std::string& filePath);
	void UnloadAudio(const std::string& filePath);
	void StopAll();

private:
	ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;
	std::unordered_map<std::string, std::shared_ptr<Audio>> audioCache_;
};

