#pragma once
#ifdef _DEBUG

#include <string>
#include <vector>

#include <core/filesystem/Path.h>

#include "core/assets/AssetID.h"
#include "core/assets/types/SequenceAuthoringData.h"

namespace Unnamed {
	class AssetManager;

	/// @brief Sequenceアセット1件分の編集状態を保持するドキュメントです。
	class SequenceEditorDocument final {
	public:
		/// @brief コンストラクタです。
		/// @param path 対象 `.sequence.json` パス
		explicit SequenceEditorDocument(const Path& path);

		/// @brief ディスクからドキュメントを再読み込みします。
		[[nodiscard]] bool ReloadFromDisk();

		/// @brief ドキュメントを保存します。
		[[nodiscard]] bool SaveToDisk();

		/// @brief Undoスタックへ現在状態を積みます。
		void PushUndoSnapshot();

		/// @brief Undoを実行します。
		[[nodiscard]] bool Undo();

		/// @brief Redoを実行します。
		[[nodiscard]] bool Redo();

		/// @brief 編集済みとしてフラグを立てます。
		void MarkDirty();

		/// @brief 外部ファイル更新の通知を受け取ります。
		void NotifyExternalChanged();

		/// @brief 外部変更競合を「ローカル保持」で解決します。
		void ResolveConflictKeepLocal();

		/// @brief 外部変更競合を「ディスク再読込」で解決します。
		[[nodiscard]] bool ResolveConflictReloadDisk();

		/// @brief 現在の編集データからプレビュー用ランタイムアセットを再生成します。
		[[nodiscard]] AssetID RebuildPreviewRuntimeAsset(
			AssetManager& assetManager
		);

		/// @brief ドキュメントパスを取得します。
		[[nodiscard]] const Path& GetPath() const;

		/// @brief ドキュメント表示名を取得します。
		[[nodiscard]] std::string GetDisplayName() const;

		/// @brief 編集データを取得します。
		[[nodiscard]] SequenceAuthoringData& GetAuthoringData();

		/// @brief 編集データを取得します（const版）。
		[[nodiscard]] const SequenceAuthoringData& GetAuthoringData() const;

		/// @brief 元ファイル由来のAssetIDを設定します。
		void SetSourceAssetId(AssetID assetId);

		/// @brief 元ファイル由来のAssetIDを取得します。
		[[nodiscard]] AssetID GetSourceAssetId() const;

		/// @brief プレビュー用ランタイムAssetIDを取得します。
		[[nodiscard]] AssetID GetPreviewAssetId() const;

		/// @brief Dirty状態を取得します。
		[[nodiscard]] bool IsDirty() const;

		/// @brief 外部変更競合状態を取得します。
		[[nodiscard]] bool HasExternalConflict() const;

		/// @brief v1->v2移行が発生したかを取得します。
		[[nodiscard]] bool WasMigratedOnLoad() const;

	private:
		Path                  mPath                  = {};
		SequenceAuthoringData mAuthoringData         = {};
		AssetID               mSourceAssetId         = kInvalidAssetID;
		AssetID               mPreviewAssetId        = kInvalidAssetID;
		uint64_t              mLastSavedSemanticHash = 0;
		bool                  mDirty                 = false;
		bool                  mExternalConflict      = false;
		bool                  mMigratedOnLoad        = false;

		std::vector<SequenceAuthoringData> mUndoStack = {};
		std::vector<SequenceAuthoringData> mRedoStack = {};
	};
}

#endif
