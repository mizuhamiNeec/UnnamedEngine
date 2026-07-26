#include "AudioSourceComponent.h"

#include <algorithm>

#include <imgui.h>

#include "engine/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/assets/types/SoundAssetData.h"
#include "core/content/ContentPathResolver.h"
#include "core/filesystem/Path.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/scene/SceneLoadOptions.h"
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
		if (
			mSoundPath.has_value() && mSoundAssetId == kInvalidAssetID
		) {
			if (AssetManager* assetManager = GetAssetManager()) {
				(void)SetSoundPath(*mSoundPath, *assetManager);
			}
		}
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
		constexpr SceneLoadOptions options{};
		const Path scenePath("<direct component deserialize>");
		const SceneDeserializeContext context{
			.loadOptions   = options,
			.assetManager  = GetAssetManager(),
			.scenePath     = scenePath,
			.entityName    = "<unknown>",
			.entityId      = 0,
			.componentType = GetStableName(),
		};
		(void)Deserialize(reader, context);
	}

	bool AudioSourceComponent::Deserialize(
		const JsonReader& reader, const SceneDeserializeContext& context
	) {
		ClearSoundPath();
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

		const JsonReader pathNode = reader["soundPath"];
		if (!pathNode.Valid()) {
			return true;
		}
		if (!pathNode.IsString()) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='invalid virtual path' scene='{}' entity='{}' entityId={} component='{}' field='soundPath' reason='expected string'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType
			);
			return !IsStrictAssetValidation(context.loadOptions);
		}

		const std::string soundPath = pathNode.GetString();
		if (soundPath.empty()) {
			return true;
		}
		const std::optional<VirtualPath> virtualPath =
			VirtualPath::ParseContentReference(soundPath);
		if (!virtualPath.has_value()) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='invalid virtual path' scene='{}' entity='{}' entityId={} component='{}' field='soundPath' virtualPath='{}'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType,
				soundPath
			);
			return !IsStrictAssetValidation(context.loadOptions);
		}

		if (!context.assetManager) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='soundPath' virtualPath='{}' reason='AssetManager unavailable'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType,
				soundPath
			);
			return !IsStrictAssetValidation(context.loadOptions);
		}

		const std::optional<ResolvedContentFile> resolvedFile =
			context.assetManager->GetContentPathResolver().ResolveFile(*virtualPath);
		if (!resolvedFile.has_value()) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='unresolved content' scene='{}' entity='{}' entityId={} component='{}' field='soundPath' virtualPath='{}'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType,
				soundPath
			);
			return !IsStrictAssetValidation(context.loadOptions);
		}

		if (SetSoundPath(*virtualPath, *context.assetManager)) {
			return true;
		}
		Error(
			kChannel,
			"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='soundPath' virtualPath='{}' mount='{}' physicalPath='{}'",
			context.scenePath,
			context.entityName,
			context.entityId,
			context.componentType,
			soundPath,
			resolvedFile->mountId,
			resolvedFile->resolvedPath
		);
		return !IsStrictAssetValidation(context.loadOptions);
	}

	void AudioSourceComponent::Serialize(JsonWriter& writer) const {
		if (mSoundPath.has_value()) {
			writer.Key("soundPath");
			writer.Write(mSoundPath->String());
		}
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
		std::string soundPath = mSoundPath.has_value() ?
			mSoundPath->String() : std::string{};
		if (
			ImGuiWidgets::AssetPathPicker(
				"Sound Path",
				soundPath,
				ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::SOUND)
			)
		) {
			const std::optional<VirtualPath> virtualPath =
				VirtualPath::ParseContentReference(soundPath);
			AssetManager* assetManager = GetAssetManager();
			if (!virtualPath.has_value() || !assetManager) {
				ClearSoundPath();
			} else {
				(void)SetSoundPath(*virtualPath, *assetManager);
			}
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

	bool AudioSourceComponent::SetSoundPath(
		const VirtualPath& path, AssetManager& assetManager
	) {
		if (
			mSoundPath.has_value() && *mSoundPath == path &&
			mSoundAssetId != kInvalidAssetID
		) {
			return true;
		}

		const AssetID assetId = assetManager.LoadSound(path);
		if (assetId == kInvalidAssetID) {
			ClearSoundPath();
			return false;
		}

		InvalidateVoice();
		mSoundPath        = path;
		mSoundAssetId     = assetId;
		mAutoPlayConsumed = false;
		mLoggedError      = false;
		return true;
	}

	void AudioSourceComponent::ClearSoundPath() noexcept {
		InvalidateVoice();
		mSoundPath.reset();
		mAutoPlayConsumed = false;
		mLoggedError      = false;
	}

	const std::optional<VirtualPath>& AudioSourceComponent::GetSoundPath()
	const noexcept {
		return mSoundPath;
	}

	AssetID AudioSourceComponent::GetSoundAssetId() const noexcept {
		return mSoundAssetId;
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
		if (!mSoundPath.has_value() || mSoundAssetId == kInvalidAssetID) {
			return false;
		}

		auto* assetManager = GetAssetManager();
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
					mSoundPath->String()
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
					mSoundPath->String()
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
