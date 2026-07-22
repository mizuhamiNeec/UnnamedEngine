#ifdef _DEBUG
#include "SequenceEditorDocument.h"
#include "core/filesystem/Path.h"

#include <algorithm>

#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/assets/loader/SequenceFileIO.h"

namespace Unnamed {
	namespace {
		constexpr size_t kMaxUndoDepth = 128;
	}

	SequenceEditorDocument::SequenceEditorDocument(const Path& path) :
		mPath(path) {
	}

	bool SequenceEditorDocument::ReloadFromDisk() {
		SequenceFileLoadResult loadResult = {};
		if (!SequenceFileIO::LoadFromFile(mPath, loadResult)) {
			return false;
		}

		mAuthoringData         = std::move(loadResult.authoring);
		mLastSavedSemanticHash = loadResult.semanticHash;
		mMigratedOnLoad        = loadResult.migrated;
		mDirty                 = false;
		mExternalConflict      = false;
		mUndoStack.clear();
		mRedoStack.clear();
		return true;
	}

	bool SequenceEditorDocument::SaveToDisk() {
		bool written = false;
		if (!SequenceFileIO::SaveToFile(mPath, mAuthoringData, &written)) {
			return false;
		}

		SequenceFileLoadResult verifyResult = {};
		if (SequenceFileIO::LoadFromFile(mPath, verifyResult)) {
			mLastSavedSemanticHash = verifyResult.semanticHash;
		}
		mDirty            = false;
		mExternalConflict = false;
		mMigratedOnLoad   = false;
		(void)written;
		return true;
	}

	void SequenceEditorDocument::PushUndoSnapshot() {
		if (mUndoStack.size() >= kMaxUndoDepth) {
			mUndoStack.erase(mUndoStack.begin());
		}
		mUndoStack.emplace_back(mAuthoringData);
		mRedoStack.clear();
	}

	bool SequenceEditorDocument::Undo() {
		if (mUndoStack.empty()) {
			return false;
		}

		mRedoStack.emplace_back(mAuthoringData);
		mAuthoringData = mUndoStack.back();
		mUndoStack.pop_back();
		MarkDirty();
		return true;
	}

	bool SequenceEditorDocument::Redo() {
		if (mRedoStack.empty()) {
			return false;
		}

		mUndoStack.emplace_back(mAuthoringData);
		mAuthoringData = mRedoStack.back();
		mRedoStack.pop_back();
		MarkDirty();
		return true;
	}

	void SequenceEditorDocument::MarkDirty() {
		mDirty = true;
	}

	void SequenceEditorDocument::NotifyExternalChanged() {
		// 未保存変更がある場合は競合状態へ移行し、即上書きは避けます。
		if (mDirty) {
			mExternalConflict = true;
			return;
		}
		(void)ReloadFromDisk();
	}

	void SequenceEditorDocument::ResolveConflictKeepLocal() {
		mExternalConflict = false;
	}

	bool SequenceEditorDocument::ResolveConflictReloadDisk() {
		return ReloadFromDisk();
	}

	AssetID SequenceEditorDocument::RebuildPreviewRuntimeAsset(
		AssetManager& assetManager
	) {
		static uint64_t   runtimeSerial = 1;
		SequenceAssetData runtimeData   = SequenceFileIO::BuildRuntimeData(
			mAuthoringData);
		const std::string runtimeName = "sequence.editor.preview." +
		                                GetDisplayName() + "." +
		                                std::to_string(runtimeSerial++);
		mPreviewAssetId = assetManager.CreateRuntimeAsset<SequenceAssetData>(
			ASSET_TYPE::SEQUENCE,
			runtimeName,
			std::move(runtimeData)
		);
		return mPreviewAssetId;
	}

	const Path& SequenceEditorDocument::GetPath() const {
		return mPath;
	}

	std::string SequenceEditorDocument::GetDisplayName() const {
		return mPath.ToUtf8();
	}

	SequenceAuthoringData& SequenceEditorDocument::GetAuthoringData() {
		return mAuthoringData;
	}

	const SequenceAuthoringData&
	SequenceEditorDocument::GetAuthoringData() const {
		return mAuthoringData;
	}

	void SequenceEditorDocument::SetSourceAssetId(const AssetID assetId) {
		mSourceAssetId = assetId;
	}

	AssetID SequenceEditorDocument::GetSourceAssetId() const {
		return mSourceAssetId;
	}

	AssetID SequenceEditorDocument::GetPreviewAssetId() const {
		return mPreviewAssetId;
	}

	bool SequenceEditorDocument::IsDirty() const {
		return mDirty;
	}

	bool SequenceEditorDocument::HasExternalConflict() const {
		return mExternalConflict;
	}

	bool SequenceEditorDocument::WasMigratedOnLoad() const {
		return mMigratedOnLoad;
	}
}

#endif
