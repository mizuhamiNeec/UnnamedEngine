#pragma once
#ifdef _DEBUG

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/assets/AssetID.h"
#include "core/assets/types/SequenceAssetData.h"
#include "core/math/Quaternion.h"
#include "core/math/Vec3.h"

#include "SequenceEditorDocument.h"
#include "SequenceEditorTypes.h"

namespace Unnamed {
	class AssetManager;
	class SequencePlayer;
	class World;

	/// @brief Sequence Editor全体のドキュメントと再生ヘッドを管理するコントローラです。
	class SequenceEditorController final {
	public:
		/// @brief コントローラを初期化します。
		void Initialize(World* world, AssetManager* assetManager);

		/// @brief プレビュー適用先Worldを切り替えます。
		void SetWorld(World* world);

		/// @brief コントローラを終了します。
		void Shutdown();

		/// @brief エディタ1フレーム分の更新を行います。
		void Tick(float deltaSeconds);

		/// @brief 指定パスのSequenceドキュメントを開きます。
		[[nodiscard]] bool OpenDocument(Path path);

		/// @brief アクティブドキュメントを保存します。
		[[nodiscard]] bool SaveActiveDocument();

		/// @brief アクティブドキュメントをUndoします。
		[[nodiscard]] bool UndoActiveDocument();

		/// @brief アクティブドキュメントをRedoします。
		[[nodiscard]] bool RedoActiveDocument();

		/// @brief アクティブドキュメントが存在するか返します。
		[[nodiscard]] bool HasActiveDocument() const;

		/// @brief アクティブドキュメントを取得します。
		[[nodiscard]] SequenceEditorDocument* GetActiveDocument();

		/// @brief アクティブドキュメントを取得します（const版）。
		[[nodiscard]] const SequenceEditorDocument* GetActiveDocument() const;

		/// @brief 開いているドキュメント一覧を取得します。
		[[nodiscard]] const std::vector<std::shared_ptr<SequenceEditorDocument>>&
		GetDocuments() const;

		/// @brief アクティブドキュメントのインデックスを取得します。
		[[nodiscard]] int32_t GetActiveDocumentIndex() const;

		/// @brief アクティブドキュメントを切り替えます。
		void SetActiveDocumentIndex(int32_t index);

		/// @brief 編集選択状態を取得します。
		[[nodiscard]] SequenceEditorSelection& GetSelection();

		/// @brief 編集選択状態を取得します（const版）。
		[[nodiscard]] const SequenceEditorSelection& GetSelection() const;

		/// @brief 再生ヘッド位置を取得します。
		[[nodiscard]] float GetPlayheadFrame() const;

		/// @brief 再生ヘッド位置を設定します。
		void SetPlayheadFrame(float frame, bool seekNow);

		/// @brief プレビュー再生を開始します。
		void PlayPreview();

		/// @brief 逆方向でプレビュー再生を開始します。
		void PlayPreviewBackward();

		/// @brief プレビュー再生を一時停止します。
		void PausePreview();

		/// @brief プレビュー再生を停止します。
		void StopPreview();

		/// @brief プレビュー再生中か返します。
		[[nodiscard]] bool IsPreviewPlaying() const;

		/// @brief AutoKey状態を取得します。
		[[nodiscard]] bool IsAutoKeyEnabled() const;

		/// @brief AutoKey状態を設定します。
		void SetAutoKeyEnabled(bool enabled);

		/// @brief Scrub時イベント発火状態を取得します。
		[[nodiscard]] bool IsScrubFireEventsEnabled() const;

		/// @brief Scrub時イベント発火状態を設定します。
		void SetScrubFireEventsEnabled(bool enabled);

		/// @brief ドキュメント編集をトランザクションとして適用します。
		[[nodiscard]] bool ModifyActiveDocument(
			const std::function<void(SequenceAuthoringData&)>& mutator
		);

		/// @brief ビューポートGizmo操作を選択中シーケンスへ反映します。
		void OnGizmoTransformChanged(
			uint64_t          entityGuid,
			const Vec3&       position,
			const Quaternion& rotation,
			const Vec3&       scale
		);

		/// @brief Asset hot reload通知を受け取りドキュメントを同期します。
		void HandleAssetReload(AssetID assetId);

	private:
		/// @brief プレビュー用プレイヤーを確保しRuntimeへ登録します。
		void EnsurePreviewPlayer();

		/// @brief アクティブドキュメントのプレビューアセットを再生成して適用します。
		void RefreshPreviewAsset();

		/// @brief 対象Entity向けのTransformトラックを解決または作成します。
		void ResolveOrCreateTransformTrackForEntity(
			SequenceAuthoringData& ioData,
			uint64_t               entityGuid,
			int32_t&               outTrackIndex,
			int32_t&               outSectionIndex
		) const;

		World*       mWorld       = nullptr;
		AssetManager* mAssetManager = nullptr;

		std::vector<std::shared_ptr<SequenceEditorDocument>> mDocuments = {};
		int32_t                   mActiveDocumentIndex = -1;
		SequenceEditorSelection   mSelection           = {};
		float                     mPlayheadFrame       = 0.0f;
		bool                      mAutoKeyEnabled      = false;
		bool                      mScrubFireEvents     = false;

		std::shared_ptr<SequencePlayer> mPreviewPlayer = nullptr;
		bool                            mPreviewRegistered = false;
	};
}

#endif
