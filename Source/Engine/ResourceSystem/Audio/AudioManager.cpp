#include "AudioManager.h"

#include <cassert>
#include <ranges>

#include "SubSystem/Console/Console.h"

AudioManager::AudioManager() {
}

AudioManager::~AudioManager() {
}

bool AudioManager::Init() {
	HRESULT hr = XAudio2Create(mXAudio2.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(hr)) {
		Console::Print("[AudioManager] XAudio2の作成に失敗しました\n", kConTextColorError, Channel::ResourceSystem);
		assert(SUCCEEDED(hr));
		return false;
	}

	// マスターボイスの作成を追加
	hr = mXAudio2->CreateMasteringVoice(&mAsterVoice);
	if (FAILED(hr)) {
		Console::Print("[AudioManager] マスターボイスの作成に失敗しました\n", kConTextColorError, Channel::ResourceSystem);
		return false;
	}

	return true;
}

void AudioManager::Shutdown() {
}

std::shared_ptr<Audio> AudioManager::GetAudio(const std::string& filePath) {
	// キャッシュを検索
	auto it = mAudioCache.find(filePath);
	if (it != mAudioCache.end()) {
		return it->second;
	}

	// 音声を新しく読み込む
	auto audio = std::make_shared<Audio>();
	if (audio->LoadFromFile(mXAudio2.Get(), filePath.c_str())) {
		mAudioCache[filePath] = audio;
		return audio;
	}

	Console::Print("[AudioManager] 音声の読み込みに失敗しました: " + filePath + "\n", kConTextColorError, Channel::ResourceSystem);

	return nullptr;
}

void AudioManager::UnloadAudio(const std::string& filePath) {
	// 検索してあったら削除
	if (mAudioCache.contains(filePath)) {
		mAudioCache.erase(filePath);
	} else {
		Console::Print("[AudioManager] 音声のアンロードに失敗しました: " + filePath + "\n", kConTextColorError, Channel::ResourceSystem);
	}
}

void AudioManager::StopAll() {
	for (const auto& audio : mAudioCache | std::views::values) {
		audio->Stop();
	}
}
