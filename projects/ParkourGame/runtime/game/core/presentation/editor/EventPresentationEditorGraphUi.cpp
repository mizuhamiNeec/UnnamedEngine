#include "EventPresentationEditorGraphUi.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <unordered_set>

#ifdef _DEBUG
#include <imgui.h>

namespace Unnamed {
	namespace {
		[[nodiscard]] ImU32 PinColor(const EventPresentationValueType type) {
			switch (type) {
				case EventPresentationValueType::Exec:
					return IM_COL32(245, 245, 245, 255);
				case EventPresentationValueType::Float:
					return IM_COL32(84, 214, 112, 255);
				case EventPresentationValueType::Bool:
					return IM_COL32(214, 92, 92, 255);
				case EventPresentationValueType::Vec2:
					return IM_COL32(88, 144, 255, 255);
				case EventPresentationValueType::Vec3:
					return IM_COL32(245, 218, 84, 255);
				case EventPresentationValueType::EntityId:
					return IM_COL32(92, 228, 236, 255);
				default: return IM_COL32(200, 200, 200, 255);
			}
		}

		[[nodiscard]] ImU32 NodeBaseColor(const EventPresentationNodeKind kind) {
			switch (kind) {
				case EventPresentationNodeKind::Trigger:
					return IM_COL32(102, 44, 44, 255);
				case EventPresentationNodeKind::Action:
					return IM_COL32(40, 62, 102, 255);
				case EventPresentationNodeKind::ValueSource:
				case EventPresentationNodeKind::ValueTransform:
					return IM_COL32(44, 92, 56, 255);
				case EventPresentationNodeKind::Comment:
					return IM_COL32(92, 88, 62, 255);
				case EventPresentationNodeKind::Reroute:
					return IM_COL32(80, 84, 90, 255);
				default: return IM_COL32(46, 52, 64, 255);
			}
		}

		[[nodiscard]] ImU32 NodeHeaderColor(const EventPresentationNodeKind kind) {
			switch (kind) {
				case EventPresentationNodeKind::Trigger:
					return IM_COL32(130, 56, 56, 255);
				case EventPresentationNodeKind::Action:
					return IM_COL32(50, 80, 132, 255);
				case EventPresentationNodeKind::ValueSource:
				case EventPresentationNodeKind::ValueTransform:
					return IM_COL32(54, 120, 72, 255);
				case EventPresentationNodeKind::Comment:
					return IM_COL32(124, 118, 78, 255);
				case EventPresentationNodeKind::Reroute:
					return IM_COL32(108, 112, 120, 255);
				default: return IM_COL32(36, 40, 48, 255);
			}
		}

		[[nodiscard]] ImU32 BrightenColor(const ImU32 color, const float amount) {
			const float t = std::clamp(amount, 0.0f, 1.0f);
			const int r = static_cast<int>(color & 0xFFu);
			const int g = static_cast<int>((color >> 8u) & 0xFFu);
			const int b = static_cast<int>((color >> 16u) & 0xFFu);
			const int a = static_cast<int>((color >> 24u) & 0xFFu);
			const int nr = static_cast<int>(std::round(r + (255 - r) * t));
			const int ng = static_cast<int>(std::round(g + (255 - g) * t));
			const int nb = static_cast<int>(std::round(b + (255 - b) * t));
			return IM_COL32(nr, ng, nb, a);
		}

		[[nodiscard]] float FadeAlpha(
			const double now,
			const double timestamp,
			const float  durationSec
		) {
			if (timestamp < 0.0 || durationSec <= 0.0f) {
				return 0.0f;
			}
			const float elapsed = static_cast<float>(now - timestamp);
			if (elapsed < 0.0f || elapsed >= durationSec) {
				return 0.0f;
			}
			return 1.0f - (elapsed / durationSec);
		}

		[[nodiscard]] const char* SourceLabel(
			const EventPresentationValueSource source
		) {
			switch (source) {
				case EventPresentationValueSource::CueValue: return "cue.value";
				case EventPresentationValueSource::CueValue2: return "cue.value2";
				case EventPresentationValueSource::PayloadFloat:
					return "payload.xxx";
				case EventPresentationValueSource::Constant: return "constant";
				default: return "cue.value";
			}
		}

		[[nodiscard]] const char* SeverityLabel(
			const EventPresentationValidationSeverity severity
		) {
			switch (severity) {
				case EventPresentationValidationSeverity::Info: return "[Info]";
				case EventPresentationValidationSeverity::Warning:
					return "[Warning]";
				case EventPresentationValidationSeverity::Error: return "[Error]";
				default: return "[Info]";
			}
		}

		[[nodiscard]] bool IsSupportedLinkValueType(
			const EventPresentationValueType valueType
		) {
			return valueType == EventPresentationValueType::Exec ||
			       valueType == EventPresentationValueType::Float;
		}

		[[nodiscard]] ImVec2 CubicBezierPoint(
			const ImVec2& p0,
			const ImVec2& p1,
			const ImVec2& p2,
			const ImVec2& p3,
			const float   t
		) {
			const float u   = 1.0f - t;
			const float tt  = t * t;
			const float uu  = u * u;
			const float uuu = uu * u;
			const float ttt = tt * t;
			return ImVec2(
				uuu * p0.x + 3.0f * uu * t * p1.x + 3.0f * u * tt * p2.x +
					ttt * p3.x,
				uuu * p0.y + 3.0f * uu * t * p1.y + 3.0f * u * tt * p2.y +
					ttt * p3.y
			);
		}

		[[nodiscard]] float PointToSegmentDistanceSq(
			const ImVec2& point,
			const ImVec2& a,
			const ImVec2& b
		) {
			const float abx = b.x - a.x;
			const float aby = b.y - a.y;
			const float apx = point.x - a.x;
			const float apy = point.y - a.y;
			const float abLenSq = abx * abx + aby * aby;
			const float t = abLenSq > 0.0f ?
				                std::clamp((apx * abx + apy * aby) / abLenSq, 0.0f, 1.0f) :
				                0.0f;
			const float px = a.x + abx * t;
			const float py = a.y + aby * t;
			const float dx = point.x - px;
			const float dy = point.y - py;
			return dx * dx + dy * dy;
		}

		[[nodiscard]] float PointToBezierDistanceSq(
			const ImVec2& point,
			const ImVec2& p0,
			const ImVec2& p1,
			const ImVec2& p2,
			const ImVec2& p3
		) {
			float  best     = FLT_MAX;
			ImVec2 previous = p0;
			constexpr int kSegments = 24;
			for (int i = 1; i <= kSegments; ++i) {
				const float t = static_cast<float>(i) / static_cast<float>(kSegments);
				const ImVec2 current = CubicBezierPoint(p0, p1, p2, p3, t);
				best = std::min(best, PointToSegmentDistanceSq(point, previous, current));
				previous = current;
			}
			return best;
		}
	}

	bool EventPresentationEditorGraphUi::Draw(
		EventPresentationEditorGraph&                      graph,
		const std::vector<EventPresentationValidationIssue>& issues
	) {
		mIssues = &issues;
		bool dirty = false;
		if (
			ImGui::BeginTable(
				"EventPresentationGraphSplit",
				2,
				ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV
			)
		) {
			ImGui::TableSetupColumn(
				"Graph",
				ImGuiTableColumnFlags_WidthStretch,
				0.75f
			);
			ImGui::TableSetupColumn(
				"Inspector",
				ImGuiTableColumnFlags_WidthStretch,
				0.25f
			);

			ImGui::TableNextColumn();
			dirty |= DrawCanvas(graph);

			ImGui::TableNextColumn();
			dirty |= DrawInspector(graph);

			ImGui::EndTable();
		}
		return dirty;
	}

	EventPresentationNodeId EventPresentationEditorGraphUi::GetSelectedNodeId()
		const {
		return mSelectedNodeId;
	}

	bool EventPresentationEditorGraphUi::IsCapturingMouseInput() const {
		return mCapturingMouseInput;
	}

	bool EventPresentationEditorGraphUi::IsGridSnapEnabled() const {
		return mGridSnapEnabled;
	}

	int EventPresentationEditorGraphUi::GetGridSnapSize() const {
		return mGridSnapSize;
	}

	void EventPresentationEditorGraphUi::SetGridSnapEnabled(const bool enabled) {
		mGridSnapEnabled = enabled;
	}

	void EventPresentationEditorGraphUi::SetGridSnapSize(const int snapSize) {
		// スナップ幅は見通しを保つため 16 単位で扱います。
		mGridSnapSize = std::max(16, (snapSize / 16) * 16);
	}

	bool EventPresentationEditorGraphUi::CanUndo() const {
		return !mUndoHistory.empty();
	}

	bool EventPresentationEditorGraphUi::CanRedo() const {
		return !mRedoHistory.empty();
	}

	bool EventPresentationEditorGraphUi::Undo(EventPresentationEditorGraph& graph) {
		if (mUndoHistory.empty()) {
			return false;
		}

		GraphSnapshot redoSnapshot = {};
		redoSnapshot.graph         = graph;
		redoSnapshot.selectedNodeIds = mSelectedNodeIds;
		redoSnapshot.selectedNodeId  = mSelectedNodeId;
		redoSnapshot.selectedLinkId  = mSelectedLinkId;
		mRedoHistory.emplace_back(std::move(redoSnapshot));
		if (mRedoHistory.size() > mHistoryLimit) {
			mRedoHistory.erase(mRedoHistory.begin());
		}

		GraphSnapshot snapshot = std::move(mUndoHistory.back());
		mUndoHistory.pop_back();
		graph           = std::move(snapshot.graph);
		mSelectedNodeIds = std::move(snapshot.selectedNodeIds);
		mSelectedNodeId  = snapshot.selectedNodeId;
		mSelectedLinkId  = snapshot.selectedLinkId;
		SanitizeSelection(graph);
		return true;
	}

	bool EventPresentationEditorGraphUi::Redo(EventPresentationEditorGraph& graph) {
		if (mRedoHistory.empty()) {
			return false;
		}

		GraphSnapshot undoSnapshot = {};
		undoSnapshot.graph         = graph;
		undoSnapshot.selectedNodeIds = mSelectedNodeIds;
		undoSnapshot.selectedNodeId  = mSelectedNodeId;
		undoSnapshot.selectedLinkId  = mSelectedLinkId;
		mUndoHistory.emplace_back(std::move(undoSnapshot));
		if (mUndoHistory.size() > mHistoryLimit) {
			mUndoHistory.erase(mUndoHistory.begin());
		}

		GraphSnapshot snapshot = std::move(mRedoHistory.back());
		mRedoHistory.pop_back();
		graph           = std::move(snapshot.graph);
		mSelectedNodeIds = std::move(snapshot.selectedNodeIds);
		mSelectedNodeId  = snapshot.selectedNodeId;
		mSelectedLinkId  = snapshot.selectedLinkId;
		SanitizeSelection(graph);
		return true;
	}

	void EventPresentationEditorGraphUi::ResetForGraph(
		const EventPresentationEditorGraph& graph
	) {
		mUndoHistory.clear();
		mRedoHistory.clear();
		mSelectedLinkId  = 0;
		mSelectedNodeId  = 0;
		mSelectedNodeIds.clear();
		mDragMode       = DragMode::None;
		mDraggingNodeId = 0;
		mDraggingPinId  = 0;
		mDragStartMouseGraphPos = Vec2(0.0f, 0.0f);
		mDragStartNodePositions.clear();
		mCapturingMouseInput = false;
		mRuntimeNodeStates.clear();
		mRuntimeExecLinkLastActivatedAt.clear();
		SanitizeSelection(graph);
	}

	void EventPresentationEditorGraphUi::NotifyTriggerExecuted(
		const EventPresentationEditorGraph& graph,
		const std::string_view              cueId
	) {
		const EventPresentationNodeId triggerNodeId = FindTriggerNodeByCueId(
			graph, cueId
		);
		if (triggerNodeId == 0) {
			return;
		}
		TouchRuntimeState(triggerNodeId, RuntimeTraceState::Executed);
	}

	void EventPresentationEditorGraphUi::NotifyTriggerConditionFailed(
		const EventPresentationEditorGraph& graph,
		const std::string_view              cueId
	) {
		const EventPresentationNodeId triggerNodeId = FindTriggerNodeByCueId(
			graph, cueId
		);
		if (triggerNodeId == 0) {
			return;
		}
		NodeRuntimeState& state = mRuntimeNodeStates[triggerNodeId];
		state.lastConditionFailedAt = ImGui::GetTime();
	}

	void EventPresentationEditorGraphUi::NotifyActionTrace(
		const EventPresentationEditorGraph& graph,
		const std::string_view              cueId,
		const size_t                        actionIndex,
		const RuntimeTraceState             state
	) {
		const EventPresentationNodeId triggerNodeId = FindTriggerNodeByCueId(
			graph, cueId
		);
		if (triggerNodeId == 0) {
			return;
		}
		const std::vector<EventPresentationNodeId> actionChain = BuildActionNodeChain(
			graph, triggerNodeId
		);
		if (actionIndex >= actionChain.size()) {
			return;
		}
		TouchRuntimeState(actionChain[actionIndex], state);

		// Exec 通過ハイライト: 対象 Action の入力 Exec リンクを一時的に強調します。
		const EventPresentationNodeData* toNode = graph.FindNode(actionChain[actionIndex]);
		if (!toNode || toNode->execInPinId == 0) {
			return;
		}
		const EventPresentationNodeId fromNodeId =
			actionIndex == 0 ? triggerNodeId : actionChain[actionIndex - 1];
		const EventPresentationNodeData* fromNode = graph.FindNode(fromNodeId);
		if (!fromNode || fromNode->execOutPinId == 0) {
			return;
		}
		for (const EventPresentationLinkData& link : graph.links) {
			if (link.fromPinId == fromNode->execOutPinId &&
			    link.toPinId == toNode->execInPinId) {
				mRuntimeExecLinkLastActivatedAt[link.id] = ImGui::GetTime();
				break;
			}
		}
	}

	bool EventPresentationEditorGraphUi::DrawCanvas(
		EventPresentationEditorGraph& graph
	) {
		bool dirty = false;
		ImGui::BeginChild(
			"EventPresentationGraphCanvas",
			ImVec2(0.0f, 0.0f),
			true,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
		);

		// キャンバス全面の不可視アイテムで入力所有権を取り、親ウィンドウへ漏らさないようにします。
		const ImVec2 canvasPos  = ImGui::GetCursorScreenPos();
		const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
		ImGui::SetCursorScreenPos(canvasPos);
		ImGui::InvisibleButton("EventPresentationGraphCanvasInput", canvasSize);
		const bool   canvasHovered = ImGui::IsItemHovered();
		const bool   canvasActive  = ImGui::IsItemActive();
		const ImVec2 canvasMin     = ImGui::GetItemRectMin();
		const ImVec2 canvasMax     = ImGui::GetItemRectMax();
		const ImVec2 mousePos      = ImGui::GetIO().MousePos;

		mCanvasOrigin = Vec2(canvasMin.x, canvasMin.y);
		mCanvasSize   = Vec2(canvasSize.x, canvasSize.y);
		mPan          = graph.viewPan;
		mZoom = std::clamp(graph.viewZoom, 0.35f, 2.5f);
		SanitizeSelection(graph);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 20, 24, 255));
		drawList->PushClipRect(canvasMin, canvasMax, true);

		if (canvasHovered && ImGui::GetIO().MouseWheel != 0.0f) {
			const float oldZoom = mZoom;
			const ImVec2 mouseGraphBefore = ScreenToGraph(mousePos);
			mZoom = std::clamp(mZoom + ImGui::GetIO().MouseWheel * 0.1f, 0.35f, 2.5f);
			const ImVec2 origin = ImVec2(mCanvasOrigin.x, mCanvasOrigin.y);
			mPan.x = (mousePos.x - origin.x) / mZoom - mouseGraphBefore.x;
			mPan.y = (mousePos.y - origin.y) / mZoom - mouseGraphBefore.y;
			if (mZoom != oldZoom) {
				dirty = true;
			}
		}

		const float gridStep = std::max(
			8.0f,
			static_cast<float>(std::max(16, mGridSnapSize)) * mZoom
		);
		const float gridOffsetX = std::fmod(mPan.x * mZoom, gridStep);
		const float gridOffsetY = std::fmod(mPan.y * mZoom, gridStep);
		for (float x = canvasMin.x + gridOffsetX; x < canvasMax.x; x += gridStep) {
			drawList->AddLine(
				ImVec2(x, canvasMin.y),
				ImVec2(x, canvasMax.y),
				IM_COL32(40, 42, 48, 255)
			);
		}
		for (float y = canvasMin.y + gridOffsetY; y < canvasMax.y; y += gridStep) {
			drawList->AddLine(
				ImVec2(canvasMin.x, y),
				ImVec2(canvasMax.x, y),
				IM_COL32(40, 42, 48, 255)
			);
		}

		struct NodeRect final {
			ImVec2 min;
			ImVec2 max;
		};
		std::unordered_map<EventPresentationNodeId, NodeRect> nodeRects;
		mPinScreenPositions.clear();
		const double nowSec = ImGui::GetTime();
		const auto drawPinGlyph = [&](const ImVec2                     pinPos,
		                              const EventPresentationValueType valueType) {
			const ImU32 pinColor = PinColor(valueType);
			if (valueType == EventPresentationValueType::Exec) {
				// Exec はデータピンより大きい横長グリフで描画し、制御フローを明確化します。
				const float halfW = 8.0f * mZoom;
				const float halfH = 5.2f * mZoom;
				drawList->AddRectFilled(
					ImVec2(pinPos.x - halfW, pinPos.y - halfH),
					ImVec2(pinPos.x + halfW, pinPos.y + halfH),
					pinColor,
					2.5f * mZoom
				);
				drawList->AddRect(
					ImVec2(pinPos.x - halfW, pinPos.y - halfH),
					ImVec2(pinPos.x + halfW, pinPos.y + halfH),
					IM_COL32(18, 18, 18, 220),
					2.5f * mZoom,
					0,
					1.2f
				);
				return;
			}
			drawList->AddCircleFilled(
				pinPos,
				4.0f * mZoom,
				pinColor
			);
		};

		for (const EventPresentationNodeData& node : graph.nodes) {
			const std::vector<EventPresentationPinId> rawInputs =
				graph.CollectPinsByDirection(
					node.id,
					EventPresentationPinDirection::Input
				);
			const std::vector<EventPresentationPinId> outputs =
				graph.CollectPinsByDirection(
					node.id,
					EventPresentationPinDirection::Output
				);
			std::vector<EventPresentationPinId> inputs;
			inputs.reserve(rawInputs.size());
			for (const EventPresentationPinId pinId : rawInputs) {
				// Trigger の Condition 入力は通常入力ピン表示から分離し、専用スロットで描画します。
				if (
					node.kind == EventPresentationNodeKind::Trigger &&
					pinId == node.floatInPinId
				) {
					continue;
				}
				inputs.emplace_back(pinId);
			}
			const bool hasConditionSlot = node.kind == EventPresentationNodeKind::Trigger &&
			                              node.floatInPinId != 0;
			const int rowCount = std::max(
				1,
				std::max(static_cast<int>(inputs.size()), static_cast<int>(outputs.size()))
			);
			const float nodeWidth = 260.0f * mZoom;
			const float headerH   = 26.0f * mZoom;
			const float rowH      = 22.0f * mZoom;
			const float conditionSlotH = hasConditionSlot ? 20.0f * mZoom : 0.0f;
			const float nodeH =
				headerH + rowCount * rowH + 10.0f * mZoom + conditionSlotH;

			const ImVec2 nodeMin = GraphToScreen(ImVec2(node.position.x, node.position.y));
			const ImVec2 nodeMax = ImVec2(nodeMin.x + nodeWidth, nodeMin.y + nodeH);
			nodeRects[node.id]   = NodeRect{.min = nodeMin, .max = nodeMax};

			for (int i = 0; i < static_cast<int>(inputs.size()); ++i) {
				const float y = nodeMin.y + headerH + (i + 0.5f) * rowH;
				mPinScreenPositions[inputs[i]] = Vec2(nodeMin.x + 8.0f * mZoom, y);
			}
			for (int i = 0; i < static_cast<int>(outputs.size()); ++i) {
				const float y = nodeMin.y + headerH + (i + 0.5f) * rowH;
				mPinScreenPositions[outputs[i]] = Vec2(
					nodeMax.x - 8.0f * mZoom,
					y
				);
			}
			if (hasConditionSlot) {
				const float conditionY = nodeMax.y - 9.0f * mZoom;
				mPinScreenPositions[node.floatInPinId] = Vec2(
					nodeMin.x + nodeWidth * 0.5f,
					conditionY
				);
			}
		}

		// 入力優先順位: Pin -> Node -> Canvas -> Window background
		EventPresentationPinId  hoveredPinId      = 0;
		EventPresentationNodeId hoveredPinNodeId  = 0;
		float                   bestPinDistanceSq = FLT_MAX;
		const float             pinHitRadius      = 10.0f * mZoom;
		const float             pinHitRadiusSq    = pinHitRadius * pinHitRadius;
		for (const auto& [pinId, pinPos] : mPinScreenPositions) {
			const float dx = mousePos.x - pinPos.x;
			const float dy = mousePos.y - pinPos.y;
			const float distanceSq = dx * dx + dy * dy;
			if (distanceSq > pinHitRadiusSq || distanceSq >= bestPinDistanceSq) {
				continue;
			}
			const EventPresentationPinData* pin = graph.FindPin(pinId);
			if (!pin) {
				continue;
			}
			bestPinDistanceSq = distanceSq;
			hoveredPinId      = pinId;
			hoveredPinNodeId  = pin->nodeId;
		}

		EventPresentationNodeId hoveredNodeId = 0;
		for (auto it = graph.nodes.rbegin(); it != graph.nodes.rend(); ++it) {
			const auto rectIt = nodeRects.find(it->id);
			if (rectIt == nodeRects.end()) {
				continue;
			}
			const NodeRect& rect = rectIt->second;
			if (
				mousePos.x >= rect.min.x && mousePos.x <= rect.max.x &&
				mousePos.y >= rect.min.y && mousePos.y <= rect.max.y
			) {
				hoveredNodeId = it->id;
				break;
			}
		}

		EventPresentationLinkId hoveredLinkId      = 0;
		float                   bestLinkDistanceSq = FLT_MAX;
		for (const EventPresentationLinkData& link : graph.links) {
			const auto fromIt = mPinScreenPositions.find(link.fromPinId);
			const auto toIt   = mPinScreenPositions.find(link.toPinId);
			if (
				fromIt == mPinScreenPositions.end() ||
				toIt == mPinScreenPositions.end()
			) {
				continue;
			}
			const ImVec2 p0 = ImVec2(fromIt->second.x, fromIt->second.y);
			const ImVec2 p3 = ImVec2(toIt->second.x, toIt->second.y);
			const EventPresentationPinData* fromPin = graph.FindPin(link.fromPinId);
			const bool isExec = fromPin &&
			                    fromPin->valueType == EventPresentationValueType::Exec;
			const float  handleLen = (isExec ? 120.0f : 80.0f) * mZoom;
			const ImVec2 p1        = ImVec2(p0.x + handleLen, p0.y);
			const ImVec2 p2        = ImVec2(p3.x - handleLen, p3.y);
			const float distanceSq =
				PointToBezierDistanceSq(mousePos, p0, p1, p2, p3);
			const float linkHitThreshold = (isExec ? 10.0f : 7.0f) * mZoom;
			const float linkHitThresholdSq = linkHitThreshold * linkHitThreshold;
			if (distanceSq > linkHitThresholdSq || distanceSq >= bestLinkDistanceSq) {
				continue;
			}
			bestLinkDistanceSq = distanceSq;
			hoveredLinkId      = link.id;
		}

		std::vector<const EventPresentationLinkData*> dataLinks;
		std::vector<const EventPresentationLinkData*> execLinks;
		dataLinks.reserve(graph.links.size());
		execLinks.reserve(graph.links.size());
		for (const EventPresentationLinkData& link : graph.links) {
			const EventPresentationPinData* fromPin = graph.FindPin(link.fromPinId);
			if (fromPin &&
			    fromPin->valueType == EventPresentationValueType::Exec) {
				execLinks.emplace_back(&link);
			} else {
				dataLinks.emplace_back(&link);
			}
		}

		const auto drawLink = [&](const EventPresentationLinkData& link,
		                          const bool                    isExec) {
			const auto fromIt = mPinScreenPositions.find(link.fromPinId);
			const auto toIt   = mPinScreenPositions.find(link.toPinId);
			if (
				fromIt == mPinScreenPositions.end() ||
				toIt == mPinScreenPositions.end()
			) {
				return;
			}
			const ImVec2 p0 = ImVec2(fromIt->second.x, fromIt->second.y);
			const ImVec2 p3 = ImVec2(toIt->second.x, toIt->second.y);
			const float  handleLen = (isExec ? 120.0f : 80.0f) * mZoom;
			const ImVec2 p1        = ImVec2(p0.x + handleLen, p0.y);
			const ImVec2 p2        = ImVec2(p3.x - handleLen, p3.y);
			const bool   selected  = mSelectedLinkId == link.id;
			const bool   hovered   = hoveredLinkId == link.id;

			ImU32 color = IM_COL32(180, 190, 220, 200);
			if (isExec) {
				// Exec は常に白系で描画し、状態表現は強度/太さで重ねます。
				color = IM_COL32(242, 242, 242, hovered ? 255 : 230);
			} else {
				const EventPresentationPinData* fromPin = graph.FindPin(link.fromPinId);
				const ImU32 typeColor = fromPin ? PinColor(fromPin->valueType) :
				                              IM_COL32(180, 190, 220, 255);
				const int alpha = hovered ? 240 : 185;
				color = IM_COL32(
					typeColor & 0xFFu,
					(typeColor >> 8u) & 0xFFu,
					(typeColor >> 16u) & 0xFFu,
					alpha
				);
			}

			float thickness = isExec ? 4.2f : 1.8f;
			if (selected) {
				thickness += isExec ? 1.2f : 0.8f;
			}
			if (hovered) {
				thickness += isExec ? 0.8f : 0.5f;
			}

			if (isExec) {
				const auto execIt = mRuntimeExecLinkLastActivatedAt.find(link.id);
				if (execIt != mRuntimeExecLinkLastActivatedAt.end()) {
					const float flowAlpha = FadeAlpha(nowSec, execIt->second, 0.8f);
					if (flowAlpha > 0.0f) {
						thickness += 1.0f * flowAlpha;
						const int add = static_cast<int>(std::round(35.0f * flowAlpha));
						const int alpha =
							std::clamp(230 + add, 0, 255);
						color = IM_COL32(255, 255, 255, alpha);
					}
				}
			}

			drawList->AddBezierCubic(p0, p1, p2, p3, color, thickness);
		};

		// データ線 -> Exec線の順で描画し、制御フローを前面に出します。
		for (const EventPresentationLinkData* link : dataLinks) {
			if (link) {
				drawLink(*link, false);
			}
		}
		for (const EventPresentationLinkData* link : execLinks) {
			if (link) {
				drawLink(*link, true);
			}
		}
		for (auto it = mRuntimeExecLinkLastActivatedAt.begin();
		     it != mRuntimeExecLinkLastActivatedAt.end();) {
			if (FadeAlpha(nowSec, it->second, 0.8f) <= 0.0f) {
				it = mRuntimeExecLinkLastActivatedAt.erase(it);
			} else {
				++it;
			}
		}

		if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
			mDragMode = DragMode::CanvasPan;
		}

		const auto canCreateLink = [&](const EventPresentationPinData* fromPin,
		                               const EventPresentationPinData* toPin) {
			if (!fromPin || !toPin) {
				return false;
			}
			if (
				fromPin->direction != EventPresentationPinDirection::Output ||
				toPin->direction != EventPresentationPinDirection::Input
			) {
				return false;
			}
			if (fromPin->nodeId == toPin->nodeId) {
				return false;
			}
			if (fromPin->valueType != toPin->valueType) {
				return false;
			}
			return IsSupportedLinkValueType(fromPin->valueType);
		};

		if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			const bool ctrlDown = ImGui::GetIO().KeyCtrl;
			if (hoveredPinId != 0) {
				const EventPresentationPinData* clickedPin = graph.FindPin(hoveredPinId);
				if (
					clickedPin &&
					clickedPin->direction == EventPresentationPinDirection::Output &&
					IsSupportedLinkValueType(clickedPin->valueType)
				) {
					mDragMode       = DragMode::LinkDrag;
					mDraggingPinId  = hoveredPinId;
					mDraggingNodeId = hoveredPinNodeId;
					SelectSingleNode(hoveredPinNodeId);
					mSelectedLinkId = 0;
				}
			} else if (hoveredNodeId != 0) {
				if (ctrlDown) {
					const auto found = std::ranges::find(
						mSelectedNodeIds, hoveredNodeId
					);
					if (found != mSelectedNodeIds.end()) {
						mSelectedNodeIds.erase(found);
					} else {
						mSelectedNodeIds.emplace_back(hoveredNodeId);
					}
					mSelectedNodeId =
						mSelectedNodeIds.empty() ? 0 : mSelectedNodeIds.front();
					mSelectedLinkId = 0;
					mDragMode       = DragMode::None;
					mDraggingNodeId = 0;
				} else {
					// ノード移動は Undo 1 ステップにまとまるよう、ドラッグ開始時点で履歴を積みます。
					PushUndoSnapshot(graph);
					if (!IsNodeSelected(hoveredNodeId)) {
						SelectSingleNode(hoveredNodeId);
					}
					mDragMode       = DragMode::NodeDrag;
					mDraggingPinId  = 0;
					mDraggingNodeId = hoveredNodeId;
					mSelectedLinkId = 0;
					const ImVec2 startGraphPos = ScreenToGraph(mousePos);
					mDragStartMouseGraphPos = Vec2(startGraphPos.x, startGraphPos.y);
					mDragStartNodePositions.clear();
					if (!mSelectedNodeIds.empty()) {
						for (const EventPresentationNodeId nodeId : mSelectedNodeIds) {
							const EventPresentationNodeData* node = graph.FindNode(nodeId);
							if (!node) {
								continue;
							}
							mDragStartNodePositions[nodeId] = node->position;
						}
					}
					if (mDragStartNodePositions.empty()) {
						const EventPresentationNodeData* draggingNode = graph.FindNode(
							hoveredNodeId
						);
						if (draggingNode) {
							mDragStartNodePositions[hoveredNodeId] =
								draggingNode->position;
						}
					}
				}
			} else if (hoveredLinkId != 0) {
				mDragMode       = DragMode::None;
				mDraggingPinId  = 0;
				mDraggingNodeId = 0;
				mSelectedNodeId = 0;
				mSelectedNodeIds.clear();
				mSelectedLinkId = hoveredLinkId;
			} else {
				mDragMode           = DragMode::SelectionRect;
				mDraggingNodeId     = 0;
				mDraggingPinId      = 0;
				const ImVec2 graphPos = ScreenToGraph(mousePos);
				mSelectionRectStart = Vec2(graphPos.x, graphPos.y);
				mSelectionRectEnd   = mSelectionRectStart;
				mSelectedNodeId     = 0;
				mSelectedNodeIds.clear();
				mSelectedLinkId     = 0;
			}
		}

		switch (mDragMode) {
			case DragMode::NodeDrag: {
				if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
					const ImVec2 currentGraphPos = ScreenToGraph(mousePos);
					const Vec2 rawDelta = Vec2(
						currentGraphPos.x - mDragStartMouseGraphPos.x,
						currentGraphPos.y - mDragStartMouseGraphPos.y
					);
					const bool temporaryDisableSnap =
						ImGui::GetIO().KeyShift || ImGui::GetIO().KeyAlt;
					Vec2 appliedDelta = rawDelta;
					if (
						mGridSnapEnabled && !temporaryDisableSnap &&
						mDragStartNodePositions.contains(mDraggingNodeId)
					) {
						// 複数移動時の相対配置を崩さないよう、先頭ドラッグノード基準でデルタをスナップします。
						const Vec2 anchorStart = mDragStartNodePositions[mDraggingNodeId];
						const Vec2 anchorTarget = Vec2(
							anchorStart.x + rawDelta.x,
							anchorStart.y + rawDelta.y
						);
						const Vec2 snappedAnchor = SnapToGrid(anchorTarget);
						appliedDelta = Vec2(
							snappedAnchor.x - anchorStart.x,
							snappedAnchor.y - anchorStart.y
						);
					}
					for (const auto& [nodeId, startPos] : mDragStartNodePositions) {
						EventPresentationNodeData* node = graph.FindNode(nodeId);
						if (!node) {
							continue;
						}
						node->position = Vec2(
							startPos.x + appliedDelta.x,
							startPos.y + appliedDelta.y
						);
						dirty = true;
					}
				} else {
					mDragMode       = DragMode::None;
					mDraggingNodeId = 0;
					mDragStartMouseGraphPos = Vec2(0.0f, 0.0f);
					mDragStartNodePositions.clear();
				}
				break;
			}
			case DragMode::CanvasPan: {
				if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
					const ImVec2 delta = ImGui::GetIO().MouseDelta;
					if (delta.x != 0.0f || delta.y != 0.0f) {
						mPan.x += delta.x / mZoom;
						mPan.y += delta.y / mZoom;
						dirty = true;
					}
				} else {
					mDragMode = DragMode::None;
				}
				break;
			}
			case DragMode::LinkDrag: {
				if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
					const EventPresentationPinData* fromPin =
						graph.FindPin(mDraggingPinId);
					const EventPresentationPinData* toPin =
						graph.FindPin(hoveredPinId);
					if (
						canCreateLink(fromPin, toPin)
					) {
						PushUndoSnapshot(graph);
						if (const EventPresentationLinkId linkId = graph.AddLink(
							    fromPin->id, toPin->id
						    );
						    linkId != 0) {
							dirty           = true;
							mSelectedLinkId = linkId;
						}
					}
					mDragMode       = DragMode::None;
					mDraggingPinId  = 0;
					mDraggingNodeId = 0;
				}
				break;
			}
			case DragMode::SelectionRect: {
				if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
					const ImVec2 graphPos = ScreenToGraph(mousePos);
					mSelectionRectEnd = Vec2(graphPos.x, graphPos.y);
				} else {
					const ImVec2 p0 = GraphToScreen(
						ImVec2(mSelectionRectStart.x, mSelectionRectStart.y)
					);
					const ImVec2 p1 = GraphToScreen(
						ImVec2(mSelectionRectEnd.x, mSelectionRectEnd.y)
					);
					const ImVec2 rectMin = ImVec2(
						std::min(p0.x, p1.x), std::min(p0.y, p1.y)
					);
					const ImVec2 rectMax = ImVec2(
						std::max(p0.x, p1.x), std::max(p0.y, p1.y)
					);
					mSelectedNodeIds.clear();
					for (const EventPresentationNodeData& node : graph.nodes) {
						const auto rectIt = nodeRects.find(node.id);
						if (rectIt == nodeRects.end()) {
							continue;
						}
						const NodeRect& nodeRect = rectIt->second;
						const bool intersects = !(nodeRect.max.x < rectMin.x ||
						                          nodeRect.min.x > rectMax.x ||
						                          nodeRect.max.y < rectMin.y ||
						                          nodeRect.min.y > rectMax.y);
						if (intersects) {
							mSelectedNodeIds.emplace_back(node.id);
						}
					}
					mSelectedNodeId =
						mSelectedNodeIds.empty() ? 0 : mSelectedNodeIds.front();
					mSelectedLinkId = 0;
					mDragMode = DragMode::None;
				}
				break;
			}
			case DragMode::None:
			default: break;
		}

		// Capture 中は次フレーム以降もマウスをグラフ側に保持します。
		mCapturingMouseInput =
			canvasHovered &&
			(canvasActive || hoveredPinId != 0 || hoveredNodeId != 0 ||
			 hoveredLinkId != 0 ||
			 mDragMode != DragMode::None);
		if (mCapturingMouseInput) {
			ImGui::SetNextFrameWantCaptureMouse(true);
		}

		if (mDragMode == DragMode::LinkDrag && mDraggingPinId != 0) {
			const auto fromIt = mPinScreenPositions.find(mDraggingPinId);
			if (fromIt != mPinScreenPositions.end()) {
				const EventPresentationPinData* draggingPin = graph.FindPin(
					mDraggingPinId
				);
				const bool isExec = draggingPin &&
				                    draggingPin->valueType ==
				                    	EventPresentationValueType::Exec;
				const ImVec2 p0 = ImVec2(fromIt->second.x, fromIt->second.y);
				const ImVec2 p3 = mousePos;
				const float  handleLen = (isExec ? 120.0f : 80.0f) * mZoom;
				const ImVec2 p1        = ImVec2(p0.x + handleLen, p0.y);
				const ImVec2 p2        = ImVec2(p3.x - handleLen, p3.y);
				const ImU32  color = isExec ? IM_COL32(250, 250, 250, 245) :
				                             IM_COL32(220, 220, 220, 240);
				const float thickness = isExec ? 4.2f : 2.0f;
				drawList->AddBezierCubic(
					p0,
					p1,
					p2,
					p3,
					color,
					thickness
				);
			}
		}

		if (mDragMode == DragMode::SelectionRect) {
			const ImVec2 p0 = GraphToScreen(
				ImVec2(mSelectionRectStart.x, mSelectionRectStart.y)
			);
			const ImVec2 p1 = GraphToScreen(
				ImVec2(mSelectionRectEnd.x, mSelectionRectEnd.y)
			);
			const ImVec2 rectMin = ImVec2(std::min(p0.x, p1.x), std::min(p0.y, p1.y));
			const ImVec2 rectMax = ImVec2(std::max(p0.x, p1.x), std::max(p0.y, p1.y));
			drawList->AddRectFilled(
				rectMin, rectMax, IM_COL32(70, 120, 200, 36), 0.0f
			);
			drawList->AddRect(
				rectMin, rectMax, IM_COL32(120, 170, 255, 180), 0.0f, 0, 1.0f
			);
		}

		if (
			mDragMode == DragMode::None &&
			!ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
			!ImGui::IsMouseDown(ImGuiMouseButton_Middle) &&
			!ImGui::IsMouseDown(ImGuiMouseButton_Right)
		) {
			mDraggingNodeId = 0;
			mDraggingPinId  = 0;
		}

		if (
			canvasHovered && hoveredLinkId != 0 &&
			ImGui::IsMouseClicked(ImGuiMouseButton_Right)
		) {
			PushUndoSnapshot(graph);
			if (graph.RemoveLink(hoveredLinkId)) {
				dirty           = true;
				mSelectedLinkId = 0;
			}
		}

		if (
			canvasHovered && hoveredPinId == 0 && hoveredNodeId == 0 &&
			hoveredLinkId == 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Right)
		) {
			const ImVec2 graphPos = ScreenToGraph(mousePos);
			mContextMenuGraphPos = Vec2(graphPos.x, graphPos.y);
			ImGui::OpenPopup("EventPresentationCanvasContext");
		}

		if (ImGui::BeginPopup("EventPresentationCanvasContext")) {
			const auto addNodeEntry = [&](const char* label,
			                              const EventPresentationNodeKind kind) {
				if (!ImGui::MenuItem(label)) {
					return;
				}
				PushUndoSnapshot(graph);
				const EventPresentationNodeId nodeId = graph.AddNode(
					kind, label, mContextMenuGraphPos
				);
				EventPresentationNodeData* node = graph.FindNode(nodeId);
				if (!node) {
					return;
				}
				switch (kind) {
					case EventPresentationNodeKind::Trigger:
						node->title        = "Trigger";
						node->execOutPinId = graph.AddPin(
							nodeId,
							"ExecOut",
							EventPresentationPinDirection::Output,
							EventPresentationValueType::Exec
						);
						node->floatInPinId = graph.AddPin(
							nodeId,
							"ConditionIn",
							EventPresentationPinDirection::Input,
							EventPresentationValueType::Float
						);
						break;
					case EventPresentationNodeKind::ValueSource:
						node->title         = "ValueSource";
						node->valueSource   = EventPresentationValueSource::CueValue;
						node->floatOutPinId = graph.AddPin(
							nodeId,
							"FloatOut",
							EventPresentationPinDirection::Output,
							EventPresentationValueType::Float
						);
						break;
					case EventPresentationNodeKind::ValueTransform:
						node->title        = "ValueTransform";
						node->floatInPinId = graph.AddPin(
							nodeId,
							"FloatIn",
							EventPresentationPinDirection::Input,
							EventPresentationValueType::Float
						);
						node->floatOutPinId = graph.AddPin(
							nodeId,
							"FloatOut",
							EventPresentationPinDirection::Output,
							EventPresentationValueType::Float
						);
						break;
					case EventPresentationNodeKind::Action:
						node->title       = "Action";
						node->actionType  = "debug.print";
						node->execInPinId = graph.AddPin(
							nodeId,
							"ExecIn",
							EventPresentationPinDirection::Input,
							EventPresentationValueType::Exec
						);
						node->execOutPinId = graph.AddPin(
							nodeId,
							"ExecOut",
							EventPresentationPinDirection::Output,
							EventPresentationValueType::Exec
						);
						node->floatInPinId = graph.AddPin(
							nodeId,
							"FloatIn",
							EventPresentationPinDirection::Input,
							EventPresentationValueType::Float
						);
						break;
					case EventPresentationNodeKind::Comment:
						node->title = "Comment";
						break;
					case EventPresentationNodeKind::Reroute:
					default: break;
				}
				SelectSingleNode(nodeId);
				mSelectedLinkId = 0;
				dirty           = true;
			};

			if (ImGui::BeginMenu("Flow")) {
				addNodeEntry("Trigger", EventPresentationNodeKind::Trigger);
				addNodeEntry("Action", EventPresentationNodeKind::Action);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Value")) {
				addNodeEntry("ValueSource", EventPresentationNodeKind::ValueSource);
				addNodeEntry(
					"ValueTransform", EventPresentationNodeKind::ValueTransform
				);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Utility")) {
				addNodeEntry("Comment", EventPresentationNodeKind::Comment);
				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}

		if (!ImGui::GetIO().WantTextInput && ImGui::GetIO().KeyCtrl) {
			if (ImGui::IsKeyPressed(ImGuiKey_Z) && !ImGui::GetIO().KeyShift) {
				if (Undo(graph)) {
					dirty = true;
				}
			} else if (
				ImGui::IsKeyPressed(ImGuiKey_Y) ||
				(ImGui::IsKeyPressed(ImGuiKey_Z) && ImGui::GetIO().KeyShift)
			) {
				if (Redo(graph)) {
					dirty = true;
				}
			}
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !ImGui::GetIO().WantTextInput) {
			if (mSelectedLinkId != 0) {
				PushUndoSnapshot(graph);
				if (graph.RemoveLink(mSelectedLinkId)) {
					dirty           = true;
					mSelectedLinkId = 0;
				}
			} else if (!mSelectedNodeIds.empty()) {
				PushUndoSnapshot(graph);
				bool removedAny = false;
				for (const EventPresentationNodeId nodeId : mSelectedNodeIds) {
					if (graph.RemoveNode(nodeId)) {
						removedAny = true;
					}
				}
				if (removedAny) {
					dirty           = true;
					mSelectedNodeId = 0;
					mSelectedNodeIds.clear();
					mSelectedLinkId = 0;
					mDragMode       = DragMode::None;
					mDraggingNodeId = 0;
					mDraggingPinId  = 0;
				}
			} else if (mSelectedNodeId != 0) {
				PushUndoSnapshot(graph);
				if (graph.RemoveNode(mSelectedNodeId)) {
					dirty           = true;
					mSelectedNodeId = 0;
					mSelectedNodeIds.clear();
					mSelectedLinkId = 0;
					mDragMode       = DragMode::None;
					mDraggingNodeId = 0;
					mDraggingPinId  = 0;
				}
			}
		}

		for (const EventPresentationNodeData& node : graph.nodes) {
			const auto rectIt = nodeRects.find(node.id);
			if (rectIt == nodeRects.end()) {
				continue;
			}
			const NodeRect& rect = rectIt->second;
			const bool      selected = IsNodeSelected(node.id);
			const ImU32 nodeColor = selected ?
				                        BrightenColor(NodeBaseColor(node.kind), 0.20f) :
				                        NodeBaseColor(node.kind);
			drawList->AddRectFilled(rect.min, rect.max, nodeColor, 6.0f);
			drawList->AddRect(
				rect.min,
				rect.max,
				selected ? IM_COL32(170, 200, 255, 255) : IM_COL32(90, 98, 116, 255),
				6.0f,
				0,
				2.0f
			);
			const float headerH = 26.0f * mZoom;
			drawList->AddRectFilled(
				rect.min,
				ImVec2(rect.max.x, rect.min.y + headerH),
				NodeHeaderColor(node.kind),
				6.0f,
				ImDrawFlags_RoundCornersTop
			);
			drawList->AddText(
				ImVec2(rect.min.x + 8.0f * mZoom, rect.min.y + 5.0f * mZoom),
				IM_COL32(230, 235, 245, 255),
				node.title.c_str()
			);

			const std::vector<EventPresentationPinId> rawInputs =
				graph.CollectPinsByDirection(
					node.id,
					EventPresentationPinDirection::Input
				);
			std::vector<EventPresentationPinId> inputs;
			inputs.reserve(rawInputs.size());
			for (const EventPresentationPinId pinId : rawInputs) {
				if (
					node.kind == EventPresentationNodeKind::Trigger &&
					pinId == node.floatInPinId
				) {
					continue;
				}
				inputs.emplace_back(pinId);
			}
			for (const EventPresentationPinId pinId : inputs) {
				const EventPresentationPinData* pin = graph.FindPin(pinId);
				const auto posIt = mPinScreenPositions.find(pinId);
				if (!pin || posIt == mPinScreenPositions.end()) {
					continue;
				}
				const ImVec2 pinPos = ImVec2(posIt->second.x, posIt->second.y);
				drawPinGlyph(pinPos, pin->valueType);
				drawList->AddText(
					ImVec2(pinPos.x + 8.0f * mZoom, pinPos.y - 7.0f * mZoom),
					IM_COL32(210, 210, 220, 255),
					pin->name.c_str()
				);
			}
			const std::vector<EventPresentationPinId> outputs =
				graph.CollectPinsByDirection(
					node.id,
					EventPresentationPinDirection::Output
				);
			for (const EventPresentationPinId pinId : outputs) {
				const EventPresentationPinData* pin = graph.FindPin(pinId);
				const auto posIt = mPinScreenPositions.find(pinId);
				if (!pin || posIt == mPinScreenPositions.end()) {
					continue;
				}
				const ImVec2 pinPos = ImVec2(posIt->second.x, posIt->second.y);
				drawPinGlyph(pinPos, pin->valueType);
				const ImVec2 textSize = ImGui::CalcTextSize(pin->name.c_str());
				drawList->AddText(
					ImVec2(
						pinPos.x - textSize.x - 10.0f * mZoom,
						pinPos.y - 7.0f * mZoom
					),
					IM_COL32(210, 210, 220, 255),
					pin->name.c_str()
				);
			}

			if (
				node.kind == EventPresentationNodeKind::Trigger &&
				node.floatInPinId != 0
			) {
				const auto condPosIt = mPinScreenPositions.find(node.floatInPinId);
				const EventPresentationPinData* condPin = graph.FindPin(
					node.floatInPinId
				);
				if (condPosIt != mPinScreenPositions.end() && condPin) {
					const ImVec2 slotCenter =
						ImVec2(condPosIt->second.x, condPosIt->second.y);
					const ImVec2 slotMin = ImVec2(
						slotCenter.x - 48.0f * mZoom,
						slotCenter.y - 12.0f * mZoom
					);
					const ImVec2 slotMax = ImVec2(
						slotCenter.x + 48.0f * mZoom,
						slotCenter.y + 4.0f * mZoom
					);
					drawList->AddRectFilled(
						slotMin, slotMax, IM_COL32(24, 28, 36, 220), 4.0f
					);
					drawList->AddRect(
						slotMin, slotMax, IM_COL32(180, 120, 120, 180), 4.0f, 0, 1.0f
					);
					drawList->AddCircleFilled(
						slotCenter,
						4.5f * mZoom,
						PinColor(condPin->valueType)
					);
					const char* slotLabel = "Condition";
					const ImVec2 labelSize = ImGui::CalcTextSize(slotLabel);
					drawList->AddText(
						ImVec2(
							slotCenter.x - labelSize.x * 0.5f,
							slotMin.y - 13.0f * mZoom
						),
						IM_COL32(235, 208, 208, 255),
						slotLabel
					);
				}
			}

			if (const auto runtimeIt = mRuntimeNodeStates.find(node.id);
			    runtimeIt != mRuntimeNodeStates.end()) {
				const NodeRuntimeState& state = runtimeIt->second;
				const float executedAlpha = FadeAlpha(nowSec, state.lastExecutedAt, 0.80f);
				const float conditionAlpha = FadeAlpha(
					nowSec, state.lastConditionFailedAt, 1.00f
				);
				const float warningAlpha = FadeAlpha(nowSec, state.lastWarningAt, 1.20f);
				const float errorAlpha = FadeAlpha(nowSec, state.lastErrorAt, 1.20f);

				if (executedAlpha > 0.0f) {
					const int a = static_cast<int>(std::round(200.0f * executedAlpha));
					drawList->AddRect(
						ImVec2(rect.min.x - 2.0f, rect.min.y - 2.0f),
						ImVec2(rect.max.x + 2.0f, rect.max.y + 2.0f),
						IM_COL32(120, 220, 255, a),
						7.0f,
						0,
						2.5f
					);
				}
				if (conditionAlpha > 0.0f) {
					const int a = static_cast<int>(std::round(220.0f * conditionAlpha));
					drawList->AddRect(
						ImVec2(rect.min.x - 3.0f, rect.min.y - 3.0f),
						ImVec2(rect.max.x + 3.0f, rect.max.y + 3.0f),
						IM_COL32(240, 110, 120, a),
						8.0f,
						0,
						2.5f
					);
				}
				if (warningAlpha > 0.0f) {
					const int a = static_cast<int>(std::round(220.0f * warningAlpha));
					drawList->AddRect(
						ImVec2(rect.min.x - 4.0f, rect.min.y - 4.0f),
						ImVec2(rect.max.x + 4.0f, rect.max.y + 4.0f),
						IM_COL32(255, 190, 92, a),
						9.0f,
						0,
						2.5f
					);
				}
				if (errorAlpha > 0.0f) {
					const int a = static_cast<int>(std::round(230.0f * errorAlpha));
					drawList->AddRect(
						ImVec2(rect.min.x - 5.0f, rect.min.y - 5.0f),
						ImVec2(rect.max.x + 5.0f, rect.max.y + 5.0f),
						IM_COL32(255, 80, 80, a),
						10.0f,
						0,
						2.5f
					);
				}
			}
		}

		std::erase_if(
			mRuntimeNodeStates,
			[&](const auto& kv) {
				const NodeRuntimeState& state = kv.second;
				const bool noHighlight = FadeAlpha(nowSec, state.lastExecutedAt, 0.80f) <=
					                        0.0f &&
				                       FadeAlpha(
					                       nowSec,
					                       state.lastConditionFailedAt,
					                       1.00f
				                       ) <= 0.0f &&
				                       FadeAlpha(nowSec, state.lastWarningAt, 1.20f) <=
					                       0.0f &&
				                       FadeAlpha(nowSec, state.lastErrorAt, 1.20f) <=
					                       0.0f;
				return noHighlight;
			}
		);

		drawList->PopClipRect();
		ImGui::EndChild();
		graph.viewPan  = mPan;
		graph.viewZoom = mZoom;
		return dirty;
	}

	bool EventPresentationEditorGraphUi::DrawInspector(
		EventPresentationEditorGraph& graph
	) {
		bool dirty = false;
		ImGui::BeginChild("EventPresentationGraphInspector", ImVec2(0.0f, 0.0f), false);
		if (mSelectedNodeId == 0) {
			ImGui::TextUnformatted("Select a node.");
		} else {
			EventPresentationNodeData* node = graph.FindNode(mSelectedNodeId);
			if (!node) {
				ImGui::TextUnformatted("Selected node is missing.");
				mSelectedNodeId = 0;
			} else {
				ImGui::Text("NodeId: %llu", static_cast<unsigned long long>(node->id));
				ImGui::Text("Kind: %d", static_cast<int>(node->kind));
				dirty |= DrawTextInput("Title", node->title, 128);
				ImGui::Separator();

				switch (node->kind) {
					case EventPresentationNodeKind::Trigger:
						dirty |= DrawTextInput("cueId", node->cueId, 256);
						dirty |= ImGui::DragFloat(
							"cooldownSec",
							&node->cooldownSec,
							0.01f,
							0.0f,
							30.0f,
							"%.3f"
						);
						dirty |= ImGui::Checkbox(
							"condition.enabled",
							&node->conditionEnabled
						);
						dirty |= ImGui::DragFloat(
							"condition.min",
							&node->conditionMin,
							0.05f,
							-1000000.0f,
							1000000.0f,
							"%.3f"
						);
						dirty |= ImGui::DragFloat(
							"condition.max",
							&node->conditionMax,
							0.05f,
							-1000000.0f,
							1000000.0f,
							"%.3f"
						);
						break;
					case EventPresentationNodeKind::Action:
						dirty |= DrawTextInput("action.type", node->actionType, 128);
						dirty |= DrawTextInput("action.id", node->actionId, 128);
						dirty |= DrawTextInput("action.debugText", node->debugText, 256);
						break;
					case EventPresentationNodeKind::ValueSource:
						dirty |= DrawValueSourceSelector(*node);
						break;
					case EventPresentationNodeKind::ValueTransform:
						dirty |= ImGui::Checkbox("clampEnabled", &node->clampEnabled);
						dirty |= ImGui::DragFloat(
							"clampMin",
							&node->clampMin,
							0.05f,
							-1000000.0f,
							1000000.0f,
							"%.3f"
						);
						dirty |= ImGui::DragFloat(
							"clampMax",
							&node->clampMax,
							0.05f,
							-1000000.0f,
							1000000.0f,
							"%.3f"
						);
						dirty |= ImGui::DragFloat(
							"multiply",
							&node->multiply,
							0.01f,
							-1000.0f,
							1000.0f,
							"%.3f"
						);
						break;
					case EventPresentationNodeKind::Comment:
						dirty |= DrawTextInput("comment", node->commentText, 256);
						break;
					case EventPresentationNodeKind::Reroute:
						ImGui::TextUnformatted("No editable property.");
						break;
					default: break;
				}
			}
		}

		ImGui::SeparatorText("Validation");
		if (!mIssues || mIssues->empty()) {
			ImGui::TextUnformatted("No issues.");
		} else {
			for (const EventPresentationValidationIssue& issue : *mIssues) {
				ImVec4 color = ImVec4(0.75f, 0.80f, 0.85f, 1.0f);
				if (issue.severity == EventPresentationValidationSeverity::Warning) {
					color = ImVec4(1.0f, 0.80f, 0.25f, 1.0f);
				} else if (issue.severity ==
				           EventPresentationValidationSeverity::Error) {
					color = ImVec4(1.0f, 0.35f, 0.30f, 1.0f);
				}
				ImGui::TextColored(
					color,
					"%s %s",
					SeverityLabel(issue.severity),
					issue.message.c_str()
				);
			}
		}

		ImGui::EndChild();
		return dirty;
	}

	bool EventPresentationEditorGraphUi::DrawValueSourceSelector(
		EventPresentationNodeData& node
	) {
		bool dirty = false;
		const char* items[] = {
			"cue.value",
			"cue.value2",
			"payload.xxx",
			"constant"
		};
		int currentIndex = 0;
		switch (node.valueSource) {
			case EventPresentationValueSource::CueValue: currentIndex = 0; break;
			case EventPresentationValueSource::CueValue2: currentIndex = 1; break;
			case EventPresentationValueSource::PayloadFloat:
				currentIndex = 2;
				break;
			case EventPresentationValueSource::Constant: currentIndex = 3; break;
			default: currentIndex = 0; break;
		}
		if (
			ImGui::Combo(
				"value.source",
				&currentIndex,
				items,
				IM_ARRAYSIZE(items)
			)
		) {
			dirty = true;
			switch (currentIndex) {
				case 0:
					node.valueSource = EventPresentationValueSource::CueValue;
					break;
				case 1:
					node.valueSource = EventPresentationValueSource::CueValue2;
					break;
				case 2:
					node.valueSource = EventPresentationValueSource::PayloadFloat;
					break;
				case 3:
					node.valueSource = EventPresentationValueSource::Constant;
					break;
				default:
					node.valueSource = EventPresentationValueSource::CueValue;
					break;
			}
		}
		ImGui::TextDisabled("Current: %s", SourceLabel(node.valueSource));
		if (node.valueSource == EventPresentationValueSource::PayloadFloat) {
			dirty |= DrawTextInput("payloadName", node.payloadName, 128);
		}
		if (node.valueSource == EventPresentationValueSource::Constant) {
			dirty |= ImGui::DragFloat(
				"constant",
				&node.constant,
				0.01f,
				-1000000.0f,
				1000000.0f,
				"%.3f"
			);
		}
		return dirty;
	}

	bool EventPresentationEditorGraphUi::DrawTextInput(
		const char*  label,
		std::string& value,
		const size_t capacity
	) {
		std::vector<char> buffer(capacity, '\0');
		const size_t      copyLength = std::min(value.size(), capacity - 1);
		if (copyLength > 0) {
			std::memcpy(buffer.data(), value.data(), copyLength);
		}
		if (!ImGui::InputText(label, buffer.data(), buffer.size())) {
			return false;
		}
		value = buffer.data();
		return true;
	}

	ImVec2 EventPresentationEditorGraphUi::GraphToScreen(
		const ImVec2& graphPos
	) const {
		return ImVec2(
			mCanvasOrigin.x + (graphPos.x + mPan.x) * mZoom,
			mCanvasOrigin.y + (graphPos.y + mPan.y) * mZoom
		);
	}

	ImVec2 EventPresentationEditorGraphUi::ScreenToGraph(
		const ImVec2& screenPos
	) const {
		return ImVec2(
			(screenPos.x - mCanvasOrigin.x) / mZoom - mPan.x,
			(screenPos.y - mCanvasOrigin.y) / mZoom - mPan.y
		);
	}

	EventPresentationNodeId EventPresentationEditorGraphUi::FindTriggerNodeByCueId(
		const EventPresentationEditorGraph& graph,
		const std::string_view              cueId
	) const {
		EventPresentationNodeId result = 0;
		for (const EventPresentationNodeData& node : graph.nodes) {
			if (node.kind != EventPresentationNodeKind::Trigger) {
				continue;
			}
			if (node.cueId != cueId) {
				continue;
			}
			if (result == 0 || node.id < result) {
				result = node.id;
			}
		}
		return result;
	}

	std::vector<EventPresentationNodeId>
	EventPresentationEditorGraphUi::BuildActionNodeChain(
		const EventPresentationEditorGraph& graph,
		const EventPresentationNodeId       triggerNodeId
	) const {
		std::vector<EventPresentationNodeId> actionNodeIds;
		const EventPresentationNodeData* triggerNode = graph.FindNode(triggerNodeId);
		if (!triggerNode || triggerNode->execOutPinId == 0) {
			return actionNodeIds;
		}

		EventPresentationPinId execCursor = triggerNode->execOutPinId;
		std::unordered_set<EventPresentationNodeId> visitedNodes;
		while (execCursor != 0) {
			const EventPresentationLinkData* foundLink = nullptr;
			for (const EventPresentationLinkData& link : graph.links) {
				if (link.fromPinId != execCursor) {
					continue;
				}
				const EventPresentationPinData* toPin = graph.FindPin(link.toPinId);
				if (!toPin) {
					continue;
				}
				const EventPresentationNodeData* toNode = graph.FindNode(toPin->nodeId);
				if (!toNode || toNode->kind != EventPresentationNodeKind::Action ||
				    toNode->execInPinId != toPin->id) {
					continue;
				}
				if (foundLink) {
					// 分岐は現行仕様で未対応のため、追跡を止めます。
					return actionNodeIds;
				}
				foundLink = &link;
			}
			if (!foundLink) {
				break;
			}
			const EventPresentationPinData* nextPin = graph.FindPin(foundLink->toPinId);
			const EventPresentationNodeData* nextNode =
				nextPin ? graph.FindNode(nextPin->nodeId) : nullptr;
			if (!nextNode) {
				break;
			}
			if (!visitedNodes.emplace(nextNode->id).second) {
				break;
			}
			actionNodeIds.emplace_back(nextNode->id);
			execCursor = nextNode->execOutPinId;
		}
		return actionNodeIds;
	}

	void EventPresentationEditorGraphUi::TouchRuntimeState(
		const EventPresentationNodeId nodeId,
		const RuntimeTraceState       state
	) {
		if (nodeId == 0) {
			return;
		}
		NodeRuntimeState& runtimeState = mRuntimeNodeStates[nodeId];
		const double now = ImGui::GetTime();
		switch (state) {
			case RuntimeTraceState::Executed:
				runtimeState.lastExecutedAt = now;
				break;
			case RuntimeTraceState::Skipped:
				runtimeState.lastExecutedAt = now;
				break;
			case RuntimeTraceState::Warning:
				runtimeState.lastWarningAt = now;
				break;
			case RuntimeTraceState::Error:
				runtimeState.lastErrorAt = now;
				break;
			default: break;
		}
	}

	void EventPresentationEditorGraphUi::PushUndoSnapshot(
		const EventPresentationEditorGraph& graph
	) {
		GraphSnapshot snapshot = {};
		snapshot.graph         = graph;
		snapshot.selectedNodeIds = mSelectedNodeIds;
		snapshot.selectedNodeId  = mSelectedNodeId;
		snapshot.selectedLinkId  = mSelectedLinkId;
		mUndoHistory.emplace_back(std::move(snapshot));
		if (mUndoHistory.size() > mHistoryLimit) {
			mUndoHistory.erase(mUndoHistory.begin());
		}
		mRedoHistory.clear();
	}

	bool EventPresentationEditorGraphUi::IsNodeSelected(
		const EventPresentationNodeId nodeId
	) const {
		return std::ranges::find(mSelectedNodeIds, nodeId) != mSelectedNodeIds.end();
	}

	void EventPresentationEditorGraphUi::SelectSingleNode(
		const EventPresentationNodeId nodeId
	) {
		mSelectedNodeId = nodeId;
		mSelectedNodeIds.clear();
		if (nodeId != 0) {
			mSelectedNodeIds.emplace_back(nodeId);
		}
	}

	void EventPresentationEditorGraphUi::SanitizeSelection(
		const EventPresentationEditorGraph& graph
	) {
		std::erase_if(
			mSelectedNodeIds,
			[&graph](const EventPresentationNodeId nodeId) {
				return graph.FindNode(nodeId) == nullptr;
			}
		);
		if (mSelectedNodeId != 0 && graph.FindNode(mSelectedNodeId) == nullptr) {
			mSelectedNodeId = 0;
		}
		if (mSelectedNodeId == 0 && !mSelectedNodeIds.empty()) {
			mSelectedNodeId = mSelectedNodeIds.front();
		}
		if (
			mSelectedLinkId != 0 &&
			std::ranges::none_of(
				graph.links,
				[this](const EventPresentationLinkData& link) {
					return link.id == mSelectedLinkId;
				}
			)
		) {
			mSelectedLinkId = 0;
		}
	}

	Vec2 EventPresentationEditorGraphUi::SnapToGrid(const Vec2& position) const {
		const float grid = static_cast<float>(std::max(16, mGridSnapSize));
		return Vec2(
			std::round(position.x / grid) * grid,
			std::round(position.y / grid) * grid
		);
	}
}

#endif
