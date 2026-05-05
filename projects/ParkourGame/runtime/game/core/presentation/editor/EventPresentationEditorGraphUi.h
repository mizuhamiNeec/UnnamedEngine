#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/math/Vec2.h"
#include "game/core/presentation/editor/EventPresentationEditorGraph.h"
#include "game/core/presentation/editor/EventPresentationEditorGraphValidator.h"

struct ImVec2;

namespace Unnamed {
	/// @brief EventPresentation 専用グラフ UI の最小描画・編集クラスです。
	class EventPresentationEditorGraphUi final {
	public:
		/// @brief Undo/Redo 用に保持するグラフ編集スナップショットです。
		struct GraphSnapshot final {
			EventPresentationEditorGraph       graph;
			std::vector<EventPresentationNodeId> selectedNodeIds;
			EventPresentationNodeId            selectedNodeId = 0;
			EventPresentationLinkId            selectedLinkId = 0;
		};

		/// @brief グラフUI内部で使用するドラッグモードです。
		enum class DragMode : uint8_t {
			None = 0,
			NodeDrag,
			CanvasPan,
			LinkDrag,
			SelectionRect
		};

		/// @brief ノード実行トレースの可視化状態です。
		enum class RuntimeTraceState : uint8_t {
			Executed = 0,
			Skipped,
			Warning,
			Error
		};

		/// @brief グラフエディタ UI を描画します。
		/// @param graph 編集対象グラフ
		/// @param issues 最新のバリデーション結果
		/// @return 変更が発生した場合 true
		[[nodiscard]] bool Draw(
			EventPresentationEditorGraph&                   graph,
			const std::vector<EventPresentationValidationIssue>& issues
		);

		/// @brief 現在選択中のノード ID を取得します。
		[[nodiscard]] EventPresentationNodeId GetSelectedNodeId() const;

		/// @brief グラフUIがマウス入力を捕捉しているか返します。
		[[nodiscard]] bool IsCapturingMouseInput() const;

		/// @brief グリッドスナップ有効状態を返します。
		[[nodiscard]] bool IsGridSnapEnabled() const;

		/// @brief グリッドスナップの幅を返します。
		[[nodiscard]] int GetGridSnapSize() const;

		/// @brief グリッドスナップ有効状態を設定します。
		/// @param enabled 有効なら true
		void SetGridSnapEnabled(bool enabled);

		/// @brief グリッドスナップ幅を設定します。
		/// @param snapSize スナップ幅（16/32/64 想定）
		void SetGridSnapSize(int snapSize);

		/// @brief Undo が可能か返します。
		[[nodiscard]] bool CanUndo() const;

		/// @brief Redo が可能か返します。
		[[nodiscard]] bool CanRedo() const;

		/// @brief 直前の編集状態へ戻します。
		/// @param graph 復元先グラフ
		/// @return 復元が行われた場合 true
		[[nodiscard]] bool Undo(EventPresentationEditorGraph& graph);

		/// @brief Undo した編集を再適用します。
		/// @param graph 復元先グラフ
		/// @return 復元が行われた場合 true
		[[nodiscard]] bool Redo(EventPresentationEditorGraph& graph);

		/// @brief 外部 rebuild 後に履歴と選択状態を初期化します。
		void ResetForGraph(const EventPresentationEditorGraph& graph);

		/// @brief Trigger が実行されたことを通知し、対応ノードをハイライトします。
		void NotifyTriggerExecuted(
			const EventPresentationEditorGraph& graph,
			std::string_view                    cueId
		);

		/// @brief Trigger の条件判定でスキップされたことを通知します。
		void NotifyTriggerConditionFailed(
			const EventPresentationEditorGraph& graph,
			std::string_view                    cueId
		);

		/// @brief Action 実行結果を通知して対応ノードをハイライトします。
		void NotifyActionTrace(
			const EventPresentationEditorGraph& graph,
			std::string_view                    cueId,
			size_t                              actionIndex,
			RuntimeTraceState                   state
		);

	private:
		/// @brief キャンバス領域を描画します。
		[[nodiscard]] bool DrawCanvas(EventPresentationEditorGraph& graph);

		/// @brief 選択ノードの Inspector を描画します。
		[[nodiscard]] bool DrawInspector(EventPresentationEditorGraph& graph);

		/// @brief 値ソース選択 UI を描画します。
		[[nodiscard]] bool DrawValueSourceSelector(
			EventPresentationNodeData& node
		);

		/// @brief ノードタイトル入力 UI を描画します。
		[[nodiscard]] bool DrawTextInput(
			const char*   label,
			std::string&  value,
			size_t        capacity = 256
		);

		[[nodiscard]] ImVec2 GraphToScreen(const ImVec2& graphPos) const;
		[[nodiscard]] ImVec2 ScreenToGraph(const ImVec2& screenPos) const;

		/// @brief 現在グラフを Undo 履歴へ積み、Redo 履歴をクリアします。
		void PushUndoSnapshot(const EventPresentationEditorGraph& graph);

		/// @brief 指定ノードIDが複数選択に含まれているか判定します。
		[[nodiscard]] bool IsNodeSelected(EventPresentationNodeId nodeId) const;

		/// @brief 単一ノード選択へ切り替えます。
		void SelectSingleNode(EventPresentationNodeId nodeId);

		/// @brief 複数選択集合から失効したノードIDを取り除きます。
		void SanitizeSelection(const EventPresentationEditorGraph& graph);

		/// @brief 指定座標を現在のスナップ幅へ吸着します。
		[[nodiscard]] Vec2 SnapToGrid(const Vec2& position) const;

		/// @brief cueId から Trigger ノード ID を解決します。
		[[nodiscard]] EventPresentationNodeId FindTriggerNodeByCueId(
			const EventPresentationEditorGraph& graph,
			std::string_view                    cueId
		) const;

		/// @brief Trigger からたどれる Action ノード列を収集します。
		[[nodiscard]] std::vector<EventPresentationNodeId> BuildActionNodeChain(
			const EventPresentationEditorGraph& graph,
			EventPresentationNodeId             triggerNodeId
		) const;

		/// @brief ノードのランタイム状態タイムスタンプを更新します。
		void TouchRuntimeState(
			EventPresentationNodeId nodeId,
			RuntimeTraceState       state
		);

		EventPresentationNodeId mSelectedNodeId = 0;
		EventPresentationLinkId mSelectedLinkId = 0;
		std::vector<EventPresentationNodeId> mSelectedNodeIds;
		float                   mZoom           = 1.0f;
		Vec2                    mPan            = Vec2(0.0f, 0.0f);
		Vec2                    mCanvasOrigin   = Vec2(0.0f, 0.0f);
		Vec2                    mCanvasSize     = Vec2(0.0f, 0.0f);
		DragMode                mDragMode       = DragMode::None;
		EventPresentationNodeId mDraggingNodeId = 0;
		EventPresentationPinId  mDraggingPinId  = 0;
		Vec2 mSelectionRectStart = Vec2(0.0f, 0.0f);
		Vec2 mSelectionRectEnd   = Vec2(0.0f, 0.0f);
		Vec2 mContextMenuGraphPos = Vec2(0.0f, 0.0f);
		Vec2 mDragStartMouseGraphPos = Vec2(0.0f, 0.0f);
		std::unordered_map<EventPresentationNodeId, Vec2> mDragStartNodePositions;
		bool mCapturingMouseInput = false;
		std::unordered_map<EventPresentationPinId, Vec2> mPinScreenPositions;
		const std::vector<EventPresentationValidationIssue>* mIssues = nullptr;
		std::vector<GraphSnapshot> mUndoHistory;
		std::vector<GraphSnapshot> mRedoHistory;
		size_t                     mHistoryLimit = 64;
		bool                       mGridSnapEnabled = true;
		int                        mGridSnapSize    = 32;

		struct NodeRuntimeState final {
			double lastExecutedAt        = -1.0;
			double lastConditionFailedAt = -1.0;
			double lastWarningAt         = -1.0;
			double lastErrorAt           = -1.0;
		};
		std::unordered_map<EventPresentationNodeId, NodeRuntimeState>
			mRuntimeNodeStates;
		std::unordered_map<EventPresentationLinkId, double>
			mRuntimeExecLinkLastActivatedAt;
	};
}
