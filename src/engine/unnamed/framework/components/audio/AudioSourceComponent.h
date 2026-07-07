#pragma once

#include <memory>
#include <optional>
#include <string>

#include "core/assets/AssetID.h"
#include "core/filesystem/VirtualPath.h"
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"

namespace Unnamed {
	class AssetManager;
	class AudioSystem;
	class AudioVoice;
	class JsonReader;
	class JsonWriter;

	class AudioSourceComponent final : public BaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override;

		[[nodiscard]] std::string_view GetComponentName() const override;

		void OnAttached() override;
		void OnDetached() override;
		void OnTick(float deltaTime) override;

		void Deserialize(const JsonReader& reader) override;
		/// @brief シーン読込方針を適用してサウンド参照を読み込みます。
		/// @return 読込を継続できる場合はtrue。
		[[nodiscard]] bool Deserialize(
			const JsonReader& reader, const SceneDeserializeContext& context
		) override;
		void Serialize(JsonWriter& writer) const override;

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		void DrawInspectorImGui() override;
#endif

		[[nodiscard]] uint32_t GetIcon() const override;

		/// @brief content内のサウンド参照をロードして設定します。
		/// @param path content-root基準の論理パス。
		/// @param assetManager サウンドをロードするAssetManager。
		/// @return サウンドをロードして設定できた場合はtrue。
		[[nodiscard]] bool SetSoundPath(
			const VirtualPath& path, AssetManager& assetManager
		);

		/// @brief サウンド参照とロード済みvoiceをクリアします。
		void ClearSoundPath() noexcept;

		/// @brief 設定中の論理サウンドパスを取得します。
		[[nodiscard]] const std::optional<VirtualPath>& GetSoundPath()
		const noexcept;

		/// @brief ロード済みサウンドAssetIDを取得します。
		[[nodiscard]] AssetID GetSoundAssetId() const noexcept;

		void               SetPlayOnStart(bool enabled) noexcept;
		[[nodiscard]] bool GetPlayOnStart() const noexcept;

		void               SetLoop(bool enabled) noexcept;
		[[nodiscard]] bool GetLoop() const noexcept;

		void                SetVolume(float volume) noexcept;
		[[nodiscard]] float GetVolume() const noexcept;

		void                SetPitch(float pitch) noexcept;
		[[nodiscard]] float GetPitch() const noexcept;

		void               Play();
		void               Stop() const;
		void               Pause() const;
		void               Resume() const;
		[[nodiscard]] bool IsPlaying() const;

	private:
		bool EnsureVoiceReady(bool preservePlayback);
		void InvalidateVoice();

		std::optional<VirtualPath> mSoundPath;
		AssetID                   mSoundAssetId       = kInvalidAssetID;
		uint64_t                  mLoadedAssetVersion = 0;

		std::shared_ptr<AudioVoice> mVoice;

		ConVar<float>* mTimeScale;

		float mVolume           = 1.0f;
		float mPitch            = 1.0f;
		bool  mPlayOnStart      = true;
		bool  mLoop             = false;
		bool  mAutoPlayConsumed = false;
		bool  mLoggedError      = false;
	};
}
