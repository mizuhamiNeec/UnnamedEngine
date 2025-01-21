#include "AudioManager.h"

#include <cassert>
#include <ranges>

#include "Lib/Console/Console.h"

AudioManager::AudioManager() {
}

AudioManager::~AudioManager() {
}

bool AudioManager::Init() {
	HRESULT hr = XAudio2Create(xAudio2_.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(hr)) {
		Console::Print("[AudioManager] XAudio2の作成に失敗しました\n", kConsoleColorError, Channel::ResourceSystem);
		assert(SUCCEEDED(hr));
		return false;
	}

	// マスターボイスの作成を追加
	hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
	if (FAILED(hr)) {
		Console::Print("[AudioManager] マスターボイスの作成に失敗しました\n", kConsoleColorError, Channel::ResourceSystem);
		return false;
	}

	return true;
}

void AudioManager::Shutdown() {
}

std::shared_ptr<Audio> AudioManager::GetAudio(const std::string& filePath) {
	// キャッシュを検索
	auto it = audioCache_.find(filePath);
	if (it != audioCache_.end()) {
		return it->second;
	}

	// 音声を新しく読み込む
	auto audio = std::make_shared<Audio>();
	if (audio->LoadFromFile(xAudio2_.Get(), filePath.c_str())) {
		audioCache_[filePath] = audio;
		return audio;
	}

	Console::Print("[AudioManager] 音声の読み込みに失敗しました: " + filePath + "\n", kConsoleColorError, Channel::ResourceSystem);

	return nullptr;
}

void AudioManager::UnloadAudio(const std::string& filePath) {
	// 検索してあったら削除
	if (audioCache_.contains(filePath)) {
		audioCache_.erase(filePath);
	} else {
		Console::Print("[AudioManager] 音声のアンロードに失敗しました: " + filePath + "\n", kConsoleColorError, Channel::ResourceSystem);
	}
}

void AudioManager::StopAll() {
	for (const auto& audio : audioCache_ | std::views::values) {
		audio->Stop();
	}
}
