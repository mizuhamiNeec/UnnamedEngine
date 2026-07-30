#pragma once
#pragma once
#include <cstdint>
#include <vector>

#include <xaudio2.h>

namespace Unnamed {
	struct SoundAssetData;

	/// @brief サウンド再生インスタンス（1ボイス）
	class AudioVoice final {
	public:
		AudioVoice() = default;
		~AudioVoice();

		AudioVoice(const AudioVoice&)            = delete;
		AudioVoice& operator=(const AudioVoice&) = delete;

		bool Init(IXAudio2* xAudio2, const SoundAssetData& soundData);

		void Play(bool isLoop = false);
		void Stop();
		void Pause();
		void Resume();
		void SetVolume(float volume) const;
		void SetPitch(float pitch) const;

		[[nodiscard]] bool IsPlaying() const;
		[[nodiscard]] bool IsPaused() const noexcept;

	private:
		void DestroyVoice();

		IXAudio2SourceVoice* mSourceVoice = nullptr;
		XAUDIO2_BUFFER       mAudioBuffer = {};
		std::vector<uint8_t> mOwnedPcmData;
		bool                 mIsPaused = false;
	};
}
