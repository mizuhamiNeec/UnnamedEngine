#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <wrl.h>

#include "Audio.h"

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
	Microsoft::WRL::ComPtr<IXAudio2> mXAudio2;
	IXAudio2MasteringVoice* mAsterVoice = nullptr;
	std::unordered_map<std::string, std::shared_ptr<Audio>> mAudioCache;
};

