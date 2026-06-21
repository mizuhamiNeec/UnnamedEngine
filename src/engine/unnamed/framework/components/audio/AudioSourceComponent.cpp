#include "AudioSourceComponent.h"

#include <algorithm>

#include <imgui.h>

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/assets/types/SoundAssetData.h"
#include "core/filesystem/Path.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/unnamed/subsystem/audio/Audio.h"
#include "engine/unnamed/subsystem/audio/AudioSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel = "AudioSrc";
	}

	std::string_view AudioSourceComponent::GetStableName() const {
		return "engine.AudioSource";
	}

	std::string_view AudioSourceComponent::GetComponentName() const {
		return "AudioSource";
	}

	void AudioSourceComponent::OnAttached() {
		mAutoPlayConsumed = false;
		mLoggedError      = false;
		(void)EnsureVoiceReady(false);

		mTimeScale = GetConsoleSystem()->GetConVarAs<ConVar<float>>(
			"host_timescale"
		);
	}

	void AudioSourceComponent::OnDetached() {
		Stop();
		InvalidateVoice();
	}

	void AudioSourceComponent::OnTick(const float) {
		if (!EnsureVoiceReady(true)) {
			return;
		}

		if (mPlayOnStart && !mAutoPlayConsumed) {
			mVoice->Play(mLoop);
			mAutoPlayConsumed = true;
		}

		if (mVoice) {
			// TimeScaleを考慮してピッチを設定する
			mVoice->SetPitch(mPitch * mTimeScale->GetValue());
		}
	}

	void AudioSourceComponent::Deserialize(const JsonReader& reader) {
		if (reader.Has("soundPath")) {
			SetSoundPath(Path(reader["soundPath"].GetString()));
		}
		if (reader.Has("playOnStart")) {
			SetPlayOnStart(reader["playOnStart"].GetBool());
		}
		if (reader.Has("loop")) {
			SetLoop(reader["loop"].GetBool());
		}
		if (reader.Has("volume")) {
			SetVolume(reader["volume"].GetFloat());
		}
		if (reader.Has("pitch")) {
			SetPitch(reader["pitch"].GetFloat());
		}
	}

	void AudioSourceComponent::Serialize(JsonWriter& writer) const {
		writer.Key("soundPath");
		writer.Write(mSoundPath.ToGenericUtf8());
		writer.Key("playOnStart");
		writer.Write(mPlayOnStart);
		writer.Key("loop");
		writer.Write(mLoop);
		writer.Key("volume");
		writer.Write(mVolume);
		writer.Key("pitch");
		writer.Write(mPitch);
	}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	void AudioSourceComponent::DrawInspectorImGui() {
		std::string soundPath = mSoundPath.ToGenericUtf8();
		if (
			ImGuiWidgets::AssetPathPicker(
				"Sound Path",
				soundPath,
				ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::SOUND)
			)
		) {
			SetSoundPath(Path(soundPath));
		}

		ImGui::Checkbox("Play On Start", &mPlayOnStart);
		ImGui::Checkbox("Loop", &mLoop);
		if (ImGui::DragFloat("Volume", &mVolume, 0.01f, 0.0f, 4.0f, "%.2f")) {
			SetVolume(mVolume);
		}
		if (ImGui::DragFloat("Pitch", &mPitch, 0.01f, 0.01f, 4.0f, "%.2f")) {
			SetPitch(mPitch);
		}

		if (ImGui::Button("Play")) {
			Play();
		}
		ImGui::SameLine();
		if (ImGui::Button("Pause")) {
			Pause();
		}
		ImGui::SameLine();
		if (ImGui::Button("Resume")) {
			Resume();
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop")) {
			Stop();
		}

		ImGui::TextUnformatted(
			IsPlaying() ? "State: Playing" : "State: Stopped"
		);
	}
#endif

	uint32_t AudioSourceComponent::GetIcon() const {
		return kIconSpeaker;
	}

	void AudioSourceComponent::SetSoundPath(Path path) {
		path = path.IsEmpty() ? Path() : path.LexicallyNormal();
		if (mSoundPath == path) {
			return;
		}
		mSoundPath        = std::move(path);
		mAutoPlayConsumed = false;
		mLoggedError      = false;
		InvalidateVoice();
	}

	const Path& AudioSourceComponent::GetSoundPath() const noexcept {
		return mSoundPath;
	}

	void AudioSourceComponent::SetPlayOnStart(const bool enabled) noexcept {
		mPlayOnStart = enabled;
	}

	bool AudioSourceComponent::GetPlayOnStart() const noexcept {
		return mPlayOnStart;
	}

	void AudioSourceComponent::SetLoop(const bool enabled) noexcept {
		mLoop = enabled;
	}

	bool AudioSourceComponent::GetLoop() const noexcept {
		return mLoop;
	}

	void AudioSourceComponent::SetVolume(const float volume) noexcept {
		mVolume = std::clamp(volume, 0.0f, 4.0f);
		if (mVoice) {
			mVoice->SetVolume(mVolume);
		}
	}

	float AudioSourceComponent::GetVolume() const noexcept {
		return mVolume;
	}

	void AudioSourceComponent::SetPitch(const float pitch) noexcept {
		mPitch = std::max(0.01f, pitch); // ピッチは0.01以上に制限
	}

	float AudioSourceComponent::GetPitch() const noexcept {
		return mPitch;
	}

	void AudioSourceComponent::Play() {
		if (!EnsureVoiceReady(false) || !mVoice) {
			return;
		}
		mVoice->Play(mLoop);
		mAutoPlayConsumed = true;
	}

	void AudioSourceComponent::Stop() const {
		if (mVoice) {
			mVoice->Stop();
		}
	}

	void AudioSourceComponent::Pause() const {
		if (mVoice) {
			mVoice->Pause();
		}
	}

	void AudioSourceComponent::Resume() const {
		if (mVoice) {
			mVoice->Resume();
		}
	}

	bool AudioSourceComponent::IsPlaying() const {
		return mVoice && mVoice->IsPlaying();
	}

	bool AudioSourceComponent::EnsureVoiceReady(const bool preservePlayback) {
		if (mSoundPath.IsEmpty()) {
			return false;
		}

		auto* assetManager = ServiceLocator::Get<AssetManager>();
		auto* audioSystem  = ServiceLocator::Get<AudioSystem>();
		if (!assetManager || !audioSystem || !audioSystem->IsReady()) {
			if (!mLoggedError) {
				Error(
					kChannel, "AssetManager or AudioSystem is not available."
				);
				mLoggedError = true;
			}
			return false;
		}

		if (mSoundAssetId == kInvalidAssetID) {
			mSoundAssetId = assetManager->LoadFromFile(
				mSoundPath, ASSET_TYPE::SOUND
			);
			mLoadedAssetVersion = 0;
		}
		if (mSoundAssetId == kInvalidAssetID) {
			if (!mLoggedError) {
				Error(kChannel, "Failed to load sound asset '{}'.", mSoundPath);
				mLoggedError = true;
			}
			return false;
		}

		const auto& meta         = assetManager->Meta(mSoundAssetId);
		const bool  needsRebuild =
			!mVoice || mLoadedAssetVersion != meta.version;
		if (!needsRebuild) {
			mLoggedError = false;
			return true;
		}

		const bool wasPlaying =
			preservePlayback && mVoice && mVoice->IsPlaying();

		const auto* soundData = assetManager->Get<
			SoundAssetData>(mSoundAssetId);
		if (!soundData) {
			if (!mLoggedError) {
				Error(
					kChannel, "Sound asset payload is invalid: '{}'.",
					mSoundPath
				);
				mLoggedError = true;
			}
			return false;
		}

		std::shared_ptr<AudioVoice> voice = audioSystem->
			CreateVoice(*soundData);
		if (!voice) {
			if (!mLoggedError) {
				Error(
					kChannel, "Failed to create audio voice for '{}'.",
					mSoundPath
				);
				mLoggedError = true;
			}
			return false;
		}

		mVoice = std::move(voice);
		mVoice->SetVolume(mVolume);
		mVoice->SetPitch(mPitch);
		mLoadedAssetVersion = meta.version;
		mLoggedError        = false;

		if (wasPlaying) {
			mVoice->Play(mLoop);
		}
		return true;
	}

	void AudioSourceComponent::InvalidateVoice() {
		mSoundAssetId       = kInvalidAssetID;
		mLoadedAssetVersion = 0;
		mVoice.reset();
	}

	REGISTER_COMPONENT(AudioSourceComponent);
}
