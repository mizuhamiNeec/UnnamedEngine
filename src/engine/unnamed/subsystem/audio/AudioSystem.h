#pragma once
#include <memory>
#include <vector>

#include <wrl/client.h>

struct IXAudio2MasteringVoice;
struct IXAudio2;

namespace Unnamed {
	class AudioVoice;
	struct SoundAssetData;

	class AudioSystem {
	public:
		AudioSystem();
		~AudioSystem();

		AudioSystem(const AudioSystem&)            = delete;
		AudioSystem& operator=(const AudioSystem&) = delete;

		/// @brief 音声出力バックエンドを初期化します。
		/// @param enableOutput false の場合は音声を再生しない無音モードで初期化します。
		/// @return 初期化に成功した場合は true を返します。
		[[nodiscard]] bool Init(bool enableOutput = true);
		void Shutdown();

		[[nodiscard]] std::shared_ptr<AudioVoice> CreateVoice(
			const SoundAssetData& soundData
		);

		void               StopAll();
		[[nodiscard]] bool IsReady() const noexcept;
		/// @brief 音声出力が有効な起動設定かどうかを返します。
		/// @details false の場合、音声コンポーネントは Voice を作成せず無音で動作します。
		[[nodiscard]] bool IsOutputEnabled() const noexcept;

	private:
		void CleanupExpiredVoices();

		Microsoft::WRL::ComPtr<IXAudio2>       mXAudio2;
		IXAudio2MasteringVoice*                mMasterVoice = nullptr;
		std::vector<std::weak_ptr<AudioVoice>> mVoices;
		bool                                   mOutputEnabled = true;
	};
}
