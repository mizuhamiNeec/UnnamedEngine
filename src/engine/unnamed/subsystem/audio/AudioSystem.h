#pragma once
#include <memory>
#include <vector>

#include <wrl/client.h>

struct IXAudio2MasteringVoice;
struct IXAudio2;

namespace Unnamed {
	class AudioVoice;
	struct SoundAssetData;

	/// @brief AudioSystemは、XAudio2 engineとmaster voiceを所有し、sound voiceの生成・停止を仲介します
	class AudioSystem {
	public:
		AudioSystem();
		~AudioSystem();

		AudioSystem(const AudioSystem&)            = delete;
		AudioSystem& operator=(const AudioSystem&) = delete;

		bool Init();
		void Shutdown();

		[[nodiscard]] std::shared_ptr<AudioVoice> CreateVoice(
			const SoundAssetData& soundData
		);

		void               StopAll();
		[[nodiscard]] bool IsReady() const noexcept;

	private:
		void CleanupExpiredVoices();

		Microsoft::WRL::ComPtr<IXAudio2>       mXAudio2;
		IXAudio2MasteringVoice*                mMasterVoice = nullptr;
		std::vector<std::weak_ptr<AudioVoice>> mVoices;
	};
}
