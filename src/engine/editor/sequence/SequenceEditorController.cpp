#ifdef _DEBUG
#include "SequenceEditorController.h"

#include <algorithm>
#include <cmath>

#include "core/assets/AssetManager.h"
#include "core/content/ContentPathResolver.h"
#include "core/guidgenerator/GuidGenerator.h"

#include "engine/content/ContentMountDefinitions.h"
#include "engine/sequence/SequencePlayer.h"
#include "engine/sequence/SequenceRuntime.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		SequenceEditorController* gSequenceEditorReloadReceiver = nullptr;
		bool                      gSequenceEditorReloadHooked   = false;

		[[nodiscard]] uint64_t AllocateStableId() {
			static GuidGenerator generator;
			return generator.Alloc();
		}

		[[nodiscard]] int64_t QuantizeFrame(const float frame) {
			return std::llround(std::max(0.0f, frame));
		}

		void EnsureCurveChannelId(SequenceRichCurveAssetData& ioCurve) {
			if (ioCurve.channelId == 0) {
				ioCurve.channelId = AllocateStableId();
			}
		}

		void AddOrUpdateFloatKey(
			SequenceRichCurveAssetData& ioCurve,
			const int64_t               frame,
			const float                 value
		) {
			EnsureCurveChannelId(ioCurve);
			for (SequenceFloatKeyAssetData& key : ioCurve.keys) {
				if (key.frame != frame) {
					continue;
				}
				key.value         = value;
				key.interpolation = SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR;
				return;
			}

			ioCurve.keys.emplace_back(
				SequenceFloatKeyAssetData{
					.keyId         = AllocateStableId(),
					.frame         = frame,
					.value         = value,
					.arriveTangent = 0.0f,
					.leaveTangent  = 0.0f,
					.interpolation = SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR,
				}
			);
			std::ranges::sort(
				ioCurve.keys,
				[](
				const SequenceFloatKeyAssetData& lhs,
				const SequenceFloatKeyAssetData& rhs
			) {
					return lhs.frame < rhs.frame;
				}
			);
		}
	}

	void SequenceEditorController::Initialize(
		World* world, AssetManager* assetManager
	) {
		mWorld        = world;
		mAssetManager = assetManager;
		EnsurePreviewPlayer();

		gSequenceEditorReloadReceiver = this;
		if (mAssetManager && !gSequenceEditorReloadHooked) {
			mAssetManager->RegisterReload(
				[](const AssetID assetId) {
					if (gSequenceEditorReloadReceiver) {
						gSequenceEditorReloadReceiver->HandleAssetReload(
							assetId);
					}
				}
			);
			gSequenceEditorReloadHooked = true;
		}
	}

	void SequenceEditorController::SetWorld(World* world) {
		if (mWorld == world) {
			return;
		}

		mWorld             = world;
		mPreviewRegistered = false;
		if (mWorld && mPreviewPlayer && !mPreviewRegistered) {
			mWorld->GetSequenceRuntime().RegisterPlayer(mPreviewPlayer);
			mPreviewRegistered = true;
		}
	}

	void SequenceEditorController::Shutdown() {
		mPreviewRegistered = false;
		mPreviewPlayer.reset();
		mDocuments.clear();
		mActiveDocumentIndex = -1;
		mSelection           = {};
		mWorld               = nullptr;
		mAssetManager        = nullptr;
		if (gSequenceEditorReloadReceiver == this) {
			gSequenceEditorReloadReceiver = nullptr;
		}
	}

	void SequenceEditorController::Tick(const float deltaSeconds) {
		(void)deltaSeconds;
		if (!mPreviewPlayer) {
			return;
		}
		if (mPreviewPlayer->GetState() == SEQUENCE_PLAYER_STATE::PLAYING) {
			mPlayheadFrame = mPreviewPlayer->GetCurrentFrame();
		}
	}

	bool SequenceEditorController::OpenDocument(Path path) {
		path = path.IsEmpty() ? Path() : path.LexicallyNormal();
		if (path.IsEmpty()) {
			return false;
		}

		for (size_t i = 0; i < mDocuments.size(); ++i) {
			if (mDocuments[i] && mDocuments[i]->GetPath() == path) {
				SetActiveDocumentIndex(static_cast<int32_t>(i));
				return true;
			}
		}

		auto document = std::make_shared<SequenceEditorDocument>(path);
		if (!document->ReloadFromDisk()) {
			Warning("SequenceEditor", "Failed to load sequence document: {}",
			        path);
			return false;
		}

		if (mAssetManager) {
			const AssetID sourceAssetId =
				mAssetManager->LoadSequenceFromFile(path);
			document->SetSourceAssetId(sourceAssetId);
		}

		mDocuments.emplace_back(std::move(document));
		SetActiveDocumentIndex(static_cast<int32_t>(mDocuments.size() - 1));
		return true;
	}

	bool SequenceEditorController::SaveActiveDocument() {
		SequenceEditorDocument* document = GetActiveDocument();
		if (!document) {
			return false;
		}
		if (mAssetManager) {
			const std::optional<std::string> mountId =
				mAssetManager->GetContentPathResolver().
				FindMountIdForResolvedPath(document->GetPath());
			if (mountId == ContentMountId::kCore) {
				Warning(
					"SequenceEditor",
					"Core Sequence assets are read-only. Duplicate the asset into Game content before saving: {}",
					document->GetPath()
				);
				return false;
			}
		}

		document->GetAuthoringData().editor.autoKeyEnabled  = mAutoKeyEnabled;
		document->GetAuthoringData().editor.scrubFireEvents = mScrubFireEvents;
		if (!document->SaveToDisk()) {
			return false;
		}

		if (mAssetManager) {
			const AssetID sourceAssetId = mAssetManager->LoadSequenceFromFile(
				document->GetPath(),
				AssetManager::AssetLoadPolicy::ForceReload
			);
			document->SetSourceAssetId(sourceAssetId);
			(void)mAssetManager->ReloadWithDependents(sourceAssetId);
		}
		return true;
	}

	bool SequenceEditorController::UndoActiveDocument() {
		SequenceEditorDocument* document = GetActiveDocument();
		if (!document) {
			return false;
		}
		if (!document->Undo()) {
			return false;
		}
		RefreshPreviewAsset();
		return true;
	}

	bool SequenceEditorController::RedoActiveDocument() {
		SequenceEditorDocument* document = GetActiveDocument();
		if (!document) {
			return false;
		}
		if (!document->Redo()) {
			return false;
		}
		RefreshPreviewAsset();
		return true;
	}

	bool SequenceEditorController::HasActiveDocument() const {
		return GetActiveDocument() != nullptr;
	}

	SequenceEditorDocument* SequenceEditorController::GetActiveDocument() {
		if (mActiveDocumentIndex < 0) {
			return nullptr;
		}
		if (mActiveDocumentIndex >= static_cast<int32_t>(mDocuments.size())) {
			return nullptr;
		}
		return mDocuments[mActiveDocumentIndex].get();
	}

	const SequenceEditorDocument*
	SequenceEditorController::GetActiveDocument() const {
		return const_cast<SequenceEditorController*>(this)->GetActiveDocument();
	}

	const std::vector<std::shared_ptr<SequenceEditorDocument>>&
	SequenceEditorController::GetDocuments() const {
		return mDocuments;
	}

	int32_t SequenceEditorController::GetActiveDocumentIndex() const {
		return mActiveDocumentIndex;
	}

	void SequenceEditorController::SetActiveDocumentIndex(const int32_t index) {
		if (index < 0 || index >= static_cast<int32_t>(mDocuments.size())) {
			return;
		}
		mActiveDocumentIndex = index;
		mSelection           = {};
		mPlayheadFrame       = 0.0f;

		if (const SequenceEditorDocument* document = GetActiveDocument()) {
			mAutoKeyEnabled = document->GetAuthoringData().editor.
			                            autoKeyEnabled;
			mScrubFireEvents = document->GetAuthoringData().editor.
			                             scrubFireEvents;
		}
		RefreshPreviewAsset();
	}

	SequenceEditorSelection& SequenceEditorController::GetSelection() {
		return mSelection;
	}

	const SequenceEditorSelection&
	SequenceEditorController::GetSelection() const {
		return mSelection;
	}

	float SequenceEditorController::GetPlayheadFrame() const {
		return mPlayheadFrame;
	}

	void SequenceEditorController::SetPlayheadFrame(
		const float frame,
		const bool  seekNow
	) {
		SequenceEditorDocument* document = GetActiveDocument();
		const float             maxFrame = document ?
			                                   static_cast<float>(
				                                   std::max<int64_t>(
					                                   0,
					                                   document->
					                                   GetAuthoringData().
					                                   lengthFrames
				                                   )
			                                   ) :
			                                   0.0f;
		mPlayheadFrame = std::clamp(frame, 0.0f, std::max(0.0f, maxFrame));

		if (!seekNow || !mPreviewPlayer || !mWorld) {
			return;
		}

		mPreviewPlayer->SetSeekEventPolicy(
			mScrubFireEvents ?
				SEQUENCE_SEEK_EVENT_POLICY::FIRE_IN_RANGE :
				SEQUENCE_SEEK_EVENT_POLICY::SUPPRESS
		);
		mPreviewPlayer->SeekFrames(mPlayheadFrame);
		mWorld->GetSequenceRuntime().EditorTick(0.0f);
	}

	void SequenceEditorController::PlayPreview() {
		EnsurePreviewPlayer();
		if (!mPreviewPlayer) {
			return;
		}
		mPreviewPlayer->SetPlaybackDirection(
			SEQUENCE_PLAYBACK_DIRECTION::FORWARD);
		mPreviewPlayer->SetSeekEventPolicy(
			mScrubFireEvents ?
				SEQUENCE_SEEK_EVENT_POLICY::FIRE_IN_RANGE :
				SEQUENCE_SEEK_EVENT_POLICY::SUPPRESS
		);
		mPreviewPlayer->Play();
	}

	void SequenceEditorController::PlayPreviewBackward() {
		EnsurePreviewPlayer();
		if (!mPreviewPlayer) {
			return;
		}
		mPreviewPlayer->SetPlaybackDirection(
			SEQUENCE_PLAYBACK_DIRECTION::BACKWARD);
		mPreviewPlayer->SetSeekEventPolicy(
			mScrubFireEvents ?
				SEQUENCE_SEEK_EVENT_POLICY::FIRE_IN_RANGE :
				SEQUENCE_SEEK_EVENT_POLICY::SUPPRESS
		);
		mPreviewPlayer->Play();
	}

	void SequenceEditorController::PausePreview() {
		if (!mPreviewPlayer) {
			return;
		}
		mPreviewPlayer->Pause();
	}

	void SequenceEditorController::StopPreview() {
		if (!mPreviewPlayer) {
			return;
		}
		mPreviewPlayer->Stop();
		SetPlayheadFrame(0.0f, true);
	}

	bool SequenceEditorController::IsPreviewPlaying() const {
		return mPreviewPlayer && mPreviewPlayer->GetState() ==
		       SEQUENCE_PLAYER_STATE::PLAYING;
	}

	bool SequenceEditorController::IsAutoKeyEnabled() const {
		return mAutoKeyEnabled;
	}

	void SequenceEditorController::SetAutoKeyEnabled(const bool enabled) {
		mAutoKeyEnabled = enabled;
		if (SequenceEditorDocument* document = GetActiveDocument()) {
			document->GetAuthoringData().editor.autoKeyEnabled = enabled;
		}
	}

	bool SequenceEditorController::IsScrubFireEventsEnabled() const {
		return mScrubFireEvents;
	}

	void SequenceEditorController::SetScrubFireEventsEnabled(
		const bool enabled
	) {
		mScrubFireEvents = enabled;
		if (SequenceEditorDocument* document = GetActiveDocument()) {
			document->GetAuthoringData().editor.scrubFireEvents = enabled;
		}
	}

	bool SequenceEditorController::ModifyActiveDocument(
		const std::function<void(SequenceAuthoringData&)>& mutator
	) {
		SequenceEditorDocument* document = GetActiveDocument();
		if (!document || !mutator) {
			return false;
		}

		document->PushUndoSnapshot();
		mutator(document->GetAuthoringData());
		document->MarkDirty();
		RefreshPreviewAsset();
		return true;
	}

	void SequenceEditorController::OnGizmoTransformChanged(
		const uint64_t    entityGuid,
		const Vec3&       position,
		const Quaternion& rotation,
		const Vec3&       scale
	) {
		if (!mAutoKeyEnabled || entityGuid == 0) {
			return;
		}

		const int64_t keyFrame = QuantizeFrame(mPlayheadFrame);
		const bool    modified = ModifyActiveDocument(
			[&](SequenceAuthoringData& data) {
				int32_t trackIndex   = -1;
				int32_t sectionIndex = -1;
				ResolveOrCreateTransformTrackForEntity(
					data,
					entityGuid,
					trackIndex,
					sectionIndex
				);
				if (
					trackIndex < 0 ||
					trackIndex >= static_cast<int32_t>(data.tracks.size())
				) {
					return;
				}

				SequenceTrackAssetData& track = data.tracks[trackIndex];
				if (
					sectionIndex < 0 ||
					sectionIndex >= static_cast<int32_t>(track.sections.size())
				) {
					return;
				}

				SequenceSectionAssetData& section = track.sections[
					sectionIndex];
				AddOrUpdateFloatKey(section.transformPosX, keyFrame,
				                    position.x);
				AddOrUpdateFloatKey(section.transformPosY, keyFrame,
				                    position.y);
				AddOrUpdateFloatKey(section.transformPosZ, keyFrame,
				                    position.z);
				AddOrUpdateFloatKey(section.transformRotX, keyFrame,
				                    rotation.x);
				AddOrUpdateFloatKey(section.transformRotY, keyFrame,
				                    rotation.y);
				AddOrUpdateFloatKey(section.transformRotZ, keyFrame,
				                    rotation.z);
				AddOrUpdateFloatKey(section.transformRotW, keyFrame,
				                    rotation.w);
				AddOrUpdateFloatKey(section.transformScaleX, keyFrame, scale.x);
				AddOrUpdateFloatKey(section.transformScaleY, keyFrame, scale.y);
				AddOrUpdateFloatKey(section.transformScaleZ, keyFrame, scale.z);

				data.lengthFrames       = std::max(data.lengthFrames, keyFrame);
				mSelection.trackIndex   = trackIndex;
				mSelection.sectionIndex = sectionIndex;
				mSelection.floatChannel =
					SEQUENCE_EDITOR_FLOAT_CHANNEL::TRANSFORM_POS_X;
			}
		);
		if (modified) {
			SetPlayheadFrame(mPlayheadFrame, true);
		}
	}

	void SequenceEditorController::HandleAssetReload(const AssetID assetId) {
		for (const auto& document : mDocuments) {
			if (!document || document->GetSourceAssetId() != assetId) {
				continue;
			}
			document->NotifyExternalChanged();
		}

		SequenceEditorDocument* activeDocument = GetActiveDocument();
		if (!activeDocument || activeDocument->HasExternalConflict()) {
			return;
		}

		mAutoKeyEnabled = activeDocument->GetAuthoringData().editor.
		                                  autoKeyEnabled;
		mScrubFireEvents = activeDocument->GetAuthoringData().editor.
		                                   scrubFireEvents;
		RefreshPreviewAsset();
	}

	void SequenceEditorController::EnsurePreviewPlayer() {
		if (!mPreviewPlayer) {
			mPreviewPlayer = std::make_shared<SequencePlayer>();
			mPreviewPlayer->SetCompletionMode(
				SEQUENCE_COMPLETION_MODE::KEEP_STATE);
			mPreviewPlayer->SetLoop(false);
			mPreviewPlayer->SetPlayRate(1.0f);
			mPreviewPlayer->SetPlaybackDirection(
				SEQUENCE_PLAYBACK_DIRECTION::FORWARD);
		}
		if (mWorld && !mPreviewRegistered) {
			mWorld->GetSequenceRuntime().RegisterPlayer(mPreviewPlayer);
			mPreviewRegistered = true;
		}
	}

	void SequenceEditorController::RefreshPreviewAsset() {
		SequenceEditorDocument* activeDocument = GetActiveDocument();
		if (!activeDocument || !mAssetManager) {
			return;
		}

		EnsurePreviewPlayer();
		if (!mPreviewPlayer) {
			return;
		}

		const AssetID previewAssetId = activeDocument->
			RebuildPreviewRuntimeAsset(
				*mAssetManager
			);
		if (previewAssetId == kInvalidAssetID) {
			return;
		}

		mPreviewPlayer->SetAssetId(previewAssetId);
		mPreviewPlayer->SetCompletionMode(SEQUENCE_COMPLETION_MODE::KEEP_STATE);
		mPreviewPlayer->SetSeekEventPolicy(
			mScrubFireEvents ?
				SEQUENCE_SEEK_EVENT_POLICY::FIRE_IN_RANGE :
				SEQUENCE_SEEK_EVENT_POLICY::SUPPRESS
		);
		mPreviewPlayer->SeekFrames(mPlayheadFrame);
		if (mWorld) {
			mWorld->GetSequenceRuntime().EditorTick(0.0f);
		}
	}

	void SequenceEditorController::ResolveOrCreateTransformTrackForEntity(
		SequenceAuthoringData& ioData,
		const uint64_t         entityGuid,
		int32_t&               outTrackIndex,
		int32_t&               outSectionIndex
	) const {
		outTrackIndex   = -1;
		outSectionIndex = -1;

		uint64_t bindingId = 0;
		for (SequenceBindingAssetData& binding : ioData.bindings) {
			if (binding.bindingId == 0) {
				binding.bindingId = AllocateStableId();
			}
			if (
				binding.entityGuid == entityGuid &&
				binding.componentStableName == "engine.Transform"
			) {
				bindingId = binding.bindingId;
				break;
			}
		}
		if (bindingId == 0) {
			SequenceBindingAssetData binding = {};
			binding.bindingId                = AllocateStableId();
			binding.entityGuid               = entityGuid;
			binding.componentStableName      = "engine.Transform";
			binding.propertyPath             = "";
			bindingId                        = binding.bindingId;
			ioData.bindings.emplace_back(std::move(binding));
		}

		if (
			mSelection.trackIndex >= 0 &&
			mSelection.trackIndex < static_cast<int32_t>(ioData.tracks.size())
		) {
			const SequenceTrackAssetData& selectedTrack = ioData.tracks[
				mSelection.trackIndex];
			if (
				selectedTrack.trackType == SEQUENCE_TRACK_TYPE::TRANSFORM &&
				selectedTrack.bindingId == bindingId
			) {
				outTrackIndex = mSelection.trackIndex;
			}
		}

		if (outTrackIndex < 0) {
			for (int32_t i = 0; i < static_cast<int32_t>(ioData.tracks.size());
			     ++i) {
				const SequenceTrackAssetData& track = ioData.tracks[i];
				if (
					track.trackType == SEQUENCE_TRACK_TYPE::TRANSFORM &&
					track.bindingId == bindingId
				) {
					outTrackIndex = i;
					break;
				}
			}
		}

		if (outTrackIndex < 0) {
			SequenceTrackAssetData track = {};
			track.trackId = AllocateStableId();
			track.name = "Transform_" + std::to_string(entityGuid);
			track.trackType = SEQUENCE_TRACK_TYPE::TRANSFORM;
			track.blendMode = SEQUENCE_BLEND_MODE::MODE_ABSOLUTE;
			track.priority = 0;
			track.bindingId = bindingId;
			SequenceSectionAssetData section = {};
			section.sectionId = AllocateStableId();
			section.startFrame = 0;
			section.endFrame = std::max<int64_t>(ioData.lengthFrames, 1);
			section.priority = 0;
			section.transformPosX.channelId = AllocateStableId();
			section.transformPosY.channelId = AllocateStableId();
			section.transformPosZ.channelId = AllocateStableId();
			section.transformRotX.channelId = AllocateStableId();
			section.transformRotY.channelId = AllocateStableId();
			section.transformRotZ.channelId = AllocateStableId();
			section.transformRotW.channelId = AllocateStableId();
			section.transformScaleX.channelId = AllocateStableId();
			section.transformScaleY.channelId = AllocateStableId();
			section.transformScaleZ.channelId = AllocateStableId();
			track.sections.emplace_back(std::move(section));
			ioData.tracks.emplace_back(std::move(track));
			outTrackIndex = static_cast<int32_t>(ioData.tracks.size() - 1);
		}

		SequenceTrackAssetData& track = ioData.tracks[outTrackIndex];
		if (track.trackId == 0) {
			track.trackId = AllocateStableId();
		}
		if (track.sections.empty()) {
			SequenceSectionAssetData section = {};
			section.sectionId = AllocateStableId();
			section.startFrame = 0;
			section.endFrame = std::max<int64_t>(ioData.lengthFrames, 1);
			track.sections.emplace_back(std::move(section));
		}

		outSectionIndex = mSelection.sectionIndex >= 0 &&
		                  mSelection.sectionIndex < static_cast<int32_t>(track.
			                  sections.size()) ?
			                  mSelection.sectionIndex :
			                  0;
		if (track.sections[outSectionIndex].sectionId == 0) {
			track.sections[outSectionIndex].sectionId = AllocateStableId();
		}
	}
}

#endif
