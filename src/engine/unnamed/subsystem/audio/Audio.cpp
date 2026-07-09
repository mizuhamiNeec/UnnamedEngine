#include "engine/unnamed/subsystem/audio/Audio.h"

#include <algorithm>

#include "core/assets/types/SoundAssetData.h"

namespace Unnamed {
	AudioVoice::AudioVoice() = default;

	AudioVoice::~AudioVoice() {
		DestroyVoice();
	}

	bool AudioVoice::Init(IXAudio2* xAudio2, const SoundAssetData& soundData) {
		DestroyVoice();

		if (!xAudio2 || soundData.pcmData.empty()) {
			return false;
		}

		WAVEFORMATEX wfex    = {};
		wfex.wFormatTag      = soundData.formatTag;
		wfex.nChannels       = soundData.channels;
		wfex.nSamplesPerSec  = soundData.sampleRate;
		wfex.nAvgBytesPerSec = soundData.averageBytesPerSecond;
		wfex.nBlockAlign     = soundData.blockAlign;
		wfex.wBitsPerSample  = soundData.bitsPerSample;
		wfex.cbSize          = 0;

		const HRESULT hr = xAudio2->CreateSourceVoice(&mSourceVoice, &wfex);
		if (FAILED(hr)) {
			DestroyVoice();
			return false;
		}

		mOwnedPcmData           = soundData.pcmData;
		mAudioBuffer            = {};
		mAudioBuffer.pAudioData = mOwnedPcmData.data();
		mAudioBuffer.AudioBytes = static_cast<UINT32>(mOwnedPcmData.size());
		mAudioBuffer.Flags      = XAUDIO2_END_OF_STREAM;
		mAudioBuffer.LoopCount  = 0;
		mIsPaused               = false;
		return true;
	}

	void AudioVoice::Play(const bool isLoop) {
		if (!mSourceVoice) {
			return;
		}

		Stop();
		mAudioBuffer.LoopCount = isLoop ? XAUDIO2_LOOP_INFINITE : 0;
		mSourceVoice->SubmitSourceBuffer(&mAudioBuffer);
		mSourceVoice->Start();
		mIsPaused = false;
	}

	void AudioVoice::Stop() {
		if (!mSourceVoice) {
			return;
		}
		mSourceVoice->Stop();
		mSourceVoice->FlushSourceBuffers();
		mIsPaused = false;
	}

	void AudioVoice::Pause() {
		if (!mSourceVoice || mIsPaused) {
			return;
		}

		if (!IsPlaying()) {
			return;
		}

		mSourceVoice->Stop();
		mIsPaused = true;
	}

	void AudioVoice::Resume() {
		if (!mSourceVoice || !mIsPaused) {
			return;
		}
		mSourceVoice->Start();
		mIsPaused = false;
	}

	void AudioVoice::SetVolume(float volume) const {
		if (!mSourceVoice) {
			return;
		}
		volume = std::clamp(volume, 0.0f, 4.0f);
		mSourceVoice->SetVolume(volume);
	}

	void AudioVoice::SetPitch(float pitch) const {
		if (!mSourceVoice) {
			return;
		}
		pitch = std::clamp(pitch, 0.01f, XAUDIO2_MAX_FREQ_RATIO);
		mSourceVoice->SetFrequencyRatio(pitch);
	}

	bool AudioVoice::IsPlaying() const {
		if (!mSourceVoice || mIsPaused) {
			return false;
		}

		XAUDIO2_VOICE_STATE state = {};
		mSourceVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
		return state.BuffersQueued > 0;
	}

	bool AudioVoice::IsPaused() const noexcept {
		return mIsPaused;
	}

	void AudioVoice::DestroyVoice() {
		if (!mSourceVoice) {
			return;
		}

		mSourceVoice->Stop();
		mSourceVoice->FlushSourceBuffers();
		mSourceVoice->DestroyVoice();
		mSourceVoice = nullptr;
		mAudioBuffer = {};
		mOwnedPcmData.clear();
		mIsPaused = false;
	}
}
