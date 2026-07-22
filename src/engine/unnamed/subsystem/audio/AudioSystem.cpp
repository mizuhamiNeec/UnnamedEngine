#include "engine/unnamed/subsystem/audio/AudioSystem.h"

#include "engine/unnamed/subsystem/audio/Audio.h"

namespace Unnamed {
	AudioSystem::AudioSystem() = default;

	AudioSystem::~AudioSystem() {
		Shutdown();
	}

	bool AudioSystem::Init(const bool enableOutput) {
		if (!enableOutput) {
			Shutdown();
			mOutputEnabled = false;
			return true;
		}

		mOutputEnabled = true;
		if (mXAudio2 && mMasterVoice) {
			return true;
		}

		HRESULT hr = XAudio2Create(
			mXAudio2.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR
		);
		if (FAILED(hr)) {
			return false;
		}

		hr = mXAudio2->CreateMasteringVoice(&mMasterVoice);
		if (FAILED(hr)) {
			mXAudio2.Reset();
			mMasterVoice = nullptr;
			return false;
		}

		return true;
	}

	void AudioSystem::Shutdown() {
		StopAll();
		mVoices.clear();

		if (mMasterVoice) {
			mMasterVoice->DestroyVoice();
			mMasterVoice = nullptr;
		}
		mXAudio2.Reset();
	}

	std::shared_ptr<AudioVoice> AudioSystem::CreateVoice(
		const SoundAssetData& soundData
	) {
		if (!mXAudio2 || !mMasterVoice) {
			return nullptr;
		}

		auto voice = std::make_shared<AudioVoice>();
		if (!voice->Init(mXAudio2.Get(), soundData)) {
			return nullptr;
		}

		// システムは弱参照だけを持ち、再生者側の寿命を不必要に延長しない
		CleanupExpiredVoices();
		mVoices.emplace_back(voice);
		return voice;
	}

	void AudioSystem::StopAll() {
		// 破棄前にキュー済みバッファの再生を止める
		CleanupExpiredVoices();
		for (const auto& weak : mVoices) {
			if (const auto voice = weak.lock()) {
				voice->Stop();
			}
		}
	}

	bool AudioSystem::IsReady() const noexcept {
		return mXAudio2 != nullptr && mMasterVoice != nullptr;
	}

	bool AudioSystem::IsOutputEnabled() const noexcept {
		return mOutputEnabled;
	}

	void AudioSystem::CleanupExpiredVoices() {
		std::erase_if(
			mVoices,
			[](const std::weak_ptr<AudioVoice>& weak) {
				return weak.expired();
			}
		);
	}
}
