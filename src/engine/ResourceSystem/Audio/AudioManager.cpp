#include <ranges>

#include "engine/ResourceSystem/Audio/AudioManager.h"

#include "engine/OldConsole/Console.h"

/// @brief コンストラクタ
AudioManager::AudioManager() {
}

/// @brief デストラクタ
AudioManager::~AudioManager() {
}

/// @brief 初期化
/// @return 成功したらtrue、失敗したらfalse
bool AudioManager::Init() {
	HRESULT hr = XAudio2Create(mXAudio2.GetAddressOf(), 0,
	                           XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(hr)) {
		Console::Print("[AudioManager] XAudio2の作成に失敗しました\n", kConTextColorError,
		               Channel::ResourceSystem);
		assert(SUCCEEDED(hr));
		return false;
	}

	// マスターボイスの作成を追加
	hr = mXAudio2->CreateMasteringVoice(&mAsterVoice);
	if (FAILED(hr)) {
		Console::Print("[AudioManager] マスターボイスの作成に失敗しました\n", kConTextColorError,
		               Channel::ResourceSystem);
		return false;
	}

	return true;
}

/// @brief シャットダウン
void AudioManager::Shutdown() {
}

/// @brief 音声を取得します
/// @param filePath 音声ファイルのパス
/// @return 音声オブジェクトへの共有ポインタ
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

	Console::Print("[AudioManager] 音声の読み込みに失敗しました: " + filePath + "\n",
	               kConTextColorError, Channel::ResourceSystem);

	return nullptr;
}

/// @brief 音声をアンロードします
/// @param filePath 音声ファイルのパス
void AudioManager::UnloadAudio(const std::string& filePath) {
	// 検索してあったら削除
	if (mAudioCache.contains(filePath)) {
		mAudioCache.erase(filePath);
	} else {
		Console::Print("[AudioManager] 音声のアンロードに失敗しました: " + filePath + "\n",
		               kConTextColorError, Channel::ResourceSystem);
	}
}

/// @brief すべての音声を停止します
void AudioManager::StopAll() {
	for (const auto& audio : mAudioCache | std::views::values) {
		audio->Stop();
	}
}
