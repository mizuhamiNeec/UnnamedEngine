#include "EventPresentationEditorGraphValidator.h"

#include <unordered_map>

namespace Unnamed {
	namespace {
		void PushIssue(
			std::vector<EventPresentationValidationIssue>& outIssues,
			const EventPresentationValidationSeverity       severity,
			const std::string&                              message,
			const EventPresentationNodeId                   nodeId = 0,
			const EventPresentationLinkId                   linkId = 0
		) {
			outIssues.emplace_back(
				EventPresentationValidationIssue{
					.severity = severity,
					.message  = message,
					.nodeId   = nodeId,
					.linkId   = linkId
				}
			);
		}
	}

	bool EventPresentationEditorGraphValidator::Validate(
		const EventPresentationEditorGraph&            graph,
		std::vector<EventPresentationValidationIssue>& outIssues
	) {
		outIssues.clear();

		size_t triggerCount = 0;
		for (const EventPresentationNodeData& node : graph.nodes) {
			if (node.kind == EventPresentationNodeKind::Trigger) {
				++triggerCount;
			}
		}
		if (triggerCount == 0) {
			PushIssue(
				outIssues,
				EventPresentationValidationSeverity::Error,
				"Graph has no Trigger node."
			);
		}

		// 入力ピンの単一入力制約チェックに使うため、toPinId ごとにリンク数を数えます。
		std::unordered_map<EventPresentationPinId, int> incomingCountByPin;
		for (const EventPresentationLinkData& link : graph.links) {
			const EventPresentationPinData* fromPin = graph.FindPin(link.fromPinId);
			const EventPresentationPinData* toPin   = graph.FindPin(link.toPinId);
			if (!fromPin || !toPin) {
				PushIssue(
					outIssues,
					EventPresentationValidationSeverity::Error,
					"Link references a missing pin.",
					0,
					link.id
				);
				continue;
			}
			if (fromPin->direction != EventPresentationPinDirection::Output ||
			    toPin->direction != EventPresentationPinDirection::Input) {
				PushIssue(
					outIssues,
					EventPresentationValidationSeverity::Error,
					"Invalid link direction. Expected output -> input.",
					0,
					link.id
				);
			}
			if (fromPin->valueType != toPin->valueType) {
				PushIssue(
					outIssues,
					EventPresentationValidationSeverity::Error,
					"Invalid link value type. Pin types must match.",
					0,
					link.id
				);
			}
			incomingCountByPin[toPin->id] += 1;
			if (incomingCountByPin[toPin->id] > 1) {
				PushIssue(
					outIssues,
					EventPresentationValidationSeverity::Error,
					"Input pin has multiple incoming links.",
					toPin->nodeId,
					link.id
				);
			}
		}

		for (const EventPresentationNodeData& node : graph.nodes) {
			if (
				node.kind == EventPresentationNodeKind::Comment ||
				node.kind == EventPresentationNodeKind::Reroute
			) {
				continue;
			}

			int touchedLinkCount = 0;
			for (const EventPresentationPinData& pin : graph.pins) {
				if (pin.nodeId != node.id) {
					continue;
				}
				for (const EventPresentationLinkData& link : graph.links) {
					if (link.fromPinId == pin.id || link.toPinId == pin.id) {
						++touchedLinkCount;
					}
				}
			}
			if (touchedLinkCount == 0) {
				PushIssue(
					outIssues,
					EventPresentationValidationSeverity::Warning,
					"Node is isolated.",
					node.id
				);
			}

			if (node.kind == EventPresentationNodeKind::Trigger) {
				if (node.cueId.empty()) {
					PushIssue(
						outIssues,
						EventPresentationValidationSeverity::Error,
						"Trigger cueId is empty.",
						node.id
					);
				}
				continue;
			}

			if (node.kind == EventPresentationNodeKind::Action) {
				if (node.actionType.empty()) {
					PushIssue(
						outIssues,
						EventPresentationValidationSeverity::Error,
						"Action type is empty.",
						node.id
					);
				}
				if (
					node.execInPinId != 0 &&
					incomingCountByPin.find(node.execInPinId) ==
					incomingCountByPin.end()
				) {
					PushIssue(
						outIssues,
						EventPresentationValidationSeverity::Error,
						"Action exec input is not connected.",
						node.id
					);
				}
				continue;
			}

			if (node.kind == EventPresentationNodeKind::ValueSource) {
				if (
					node.valueSource == EventPresentationValueSource::PayloadFloat &&
					node.payloadName.empty()
				) {
					PushIssue(
						outIssues,
						EventPresentationValidationSeverity::Error,
						"ValueSource payload.xxx requires payloadName.",
						node.id
					);
				}
			}
		}

		for (const EventPresentationValidationIssue& issue : outIssues) {
			if (issue.severity == EventPresentationValidationSeverity::Error) {
				return false;
			}
		}
		return true;
	}
}

