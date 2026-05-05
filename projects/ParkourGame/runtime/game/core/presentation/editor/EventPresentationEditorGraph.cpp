#include "EventPresentationEditorGraph.h"

#include <algorithm>
#include <cassert>

namespace Unnamed {
	void EventPresentationEditorGraph::Clear() {
		name.clear();
		viewPan  = Vec2(0.0f, 0.0f);
		viewZoom = 1.0f;
		nodes.clear();
		pins.clear();
		links.clear();
		mNextNodeId = 1;
		mNextPinId  = 1;
		mNextLinkId = 1;
	}

	EventPresentationNodeId EventPresentationEditorGraph::AddNode(
		const EventPresentationNodeKind kind,
		const std::string&              title,
		const Vec2&                     position
	) {
		const EventPresentationNodeId id = mNextNodeId++;
		EventPresentationNodeData     node = {};
		node.id                           = id;
		node.kind                         = kind;
		node.title                        = title;
		node.position                     = position;
		nodes.emplace_back(std::move(node));
		return id;
	}

	EventPresentationPinId EventPresentationEditorGraph::AddPin(
		const EventPresentationNodeId       nodeId,
		const std::string&                  nameValue,
		const EventPresentationPinDirection direction,
		const EventPresentationValueType    valueType
	) {
		if (nodeId == 0) {
			return 0;
		}
		EventPresentationNodeData* node = FindNode(nodeId);
		if (!node) {
			return 0;
		}

		const EventPresentationPinId id = mNextPinId++;
		EventPresentationPinData     pin = {};
		pin.id                           = id;
		pin.nodeId                       = nodeId;
		pin.name                         = nameValue;
		pin.direction                    = direction;
		pin.valueType                    = valueType;
		pins.emplace_back(std::move(pin));
		return id;
	}

	EventPresentationLinkId EventPresentationEditorGraph::AddLink(
		const EventPresentationPinId fromPinId,
		const EventPresentationPinId toPinId
	) {
		if (fromPinId == 0 || toPinId == 0) {
			return 0;
		}
		const EventPresentationPinData* fromPin = FindPin(fromPinId);
		const EventPresentationPinData* toPin   = FindPin(toPinId);
		if (!fromPin || !toPin) {
			return 0;
		}
		if (
			fromPin->direction != EventPresentationPinDirection::Output ||
			toPin->direction != EventPresentationPinDirection::Input
		) {
			return 0;
		}
		// 自己接続や型不一致は作成時点で拒否して、Validator の制約と揃えます。
		if (fromPin->nodeId == toPin->nodeId) {
			return 0;
		}
		if (
			fromPin->valueType != toPin->valueType ||
			(fromPin->valueType != EventPresentationValueType::Exec &&
			 fromPin->valueType != EventPresentationValueType::Float)
		) {
			return 0;
		}
		const auto duplicate = std::ranges::find_if(
			links,
			[fromPinId, toPinId](const EventPresentationLinkData& link) {
				return link.fromPinId == fromPinId && link.toPinId == toPinId;
			}
		);
		if (duplicate != links.end()) {
			return 0;
		}
		if (fromPin->valueType == EventPresentationValueType::Exec) {
			const auto existingOutgoingExec = std::ranges::find_if(
				links,
				[fromPinId](const EventPresentationLinkData& link) {
					return link.fromPinId == fromPinId;
				}
			);
			if (existingOutgoingExec != links.end()) {
				return 0;
			}
		}
		const auto existingIncoming = std::ranges::find_if(
			links,
			[toPinId](const EventPresentationLinkData& link) {
				return link.toPinId == toPinId;
			}
		);
		if (existingIncoming != links.end()) {
			return 0;
		}

		const EventPresentationLinkId id = mNextLinkId++;
		EventPresentationLinkData     link = {};
		link.id                            = id;
		link.fromPinId                     = fromPinId;
		link.toPinId                       = toPinId;
		links.emplace_back(std::move(link));
		return id;
	}

	bool EventPresentationEditorGraph::RemoveLink(
		const EventPresentationLinkId linkId
	) {
		if (linkId == 0) {
			return false;
		}
		const auto before = links.size();
		std::erase_if(
			links,
			[linkId](const EventPresentationLinkData& link) {
				return link.id == linkId;
			}
		);
		return links.size() != before;
	}

	bool EventPresentationEditorGraph::RemoveNode(
		const EventPresentationNodeId nodeId
	) {
		if (nodeId == 0) {
			return false;
		}
		const auto nodeIt = std::ranges::find_if(
			nodes,
			[nodeId](const EventPresentationNodeData& node) {
				return node.id == nodeId;
			}
		);
		if (nodeIt == nodes.end()) {
			return false;
		}

		std::vector<EventPresentationPinId> removedPinIds;
		removedPinIds.reserve(8);
		for (const EventPresentationPinData& pin : pins) {
			if (pin.nodeId == nodeId) {
				removedPinIds.emplace_back(pin.id);
			}
		}
		std::erase_if(
			links,
			[&removedPinIds](const EventPresentationLinkData& link) {
				return std::ranges::find(removedPinIds, link.fromPinId) !=
					       removedPinIds.end() ||
				       std::ranges::find(removedPinIds, link.toPinId) !=
					       removedPinIds.end();
			}
		);
		std::erase_if(
			pins,
			[nodeId](const EventPresentationPinData& pin) {
				return pin.nodeId == nodeId;
			}
		);
		nodes.erase(nodeIt);
		return true;
	}

	EventPresentationNodeData* EventPresentationEditorGraph::FindNode(
		const EventPresentationNodeId id
	) {
		assert(id != 0 && "FindNode called with invalid node id.");
		if (id == 0) {
			return nullptr;
		}
		const auto found = std::ranges::find_if(
			nodes,
			[id](const EventPresentationNodeData& node) {
				return node.id == id;
			}
		);
		return found != nodes.end() ? &(*found) : nullptr;
	}

	const EventPresentationNodeData* EventPresentationEditorGraph::FindNode(
		const EventPresentationNodeId id
	) const {
		assert(id != 0 && "FindNode called with invalid node id.");
		if (id == 0) {
			return nullptr;
		}
		const auto found = std::ranges::find_if(
			nodes,
			[id](const EventPresentationNodeData& node) {
				return node.id == id;
			}
		);
		return found != nodes.end() ? &(*found) : nullptr;
	}

	EventPresentationPinData* EventPresentationEditorGraph::FindPin(
		const EventPresentationPinId id
	) {
		const auto found = std::ranges::find_if(
			pins,
			[id](const EventPresentationPinData& pin) {
				return pin.id == id;
			}
		);
		return found != pins.end() ? &(*found) : nullptr;
	}

	const EventPresentationPinData* EventPresentationEditorGraph::FindPin(
		const EventPresentationPinId id
	) const {
		const auto found = std::ranges::find_if(
			pins,
			[id](const EventPresentationPinData& pin) {
				return pin.id == id;
			}
		);
		return found != pins.end() ? &(*found) : nullptr;
	}

	std::vector<EventPresentationPinId>
	EventPresentationEditorGraph::CollectPinsByDirection(
		const EventPresentationNodeId       nodeId,
		const EventPresentationPinDirection direction
	) const {
		std::vector<EventPresentationPinId> result;
		for (const EventPresentationPinData& pin : pins) {
			if (pin.nodeId != nodeId || pin.direction != direction) {
				continue;
			}
			result.emplace_back(pin.id);
		}
		return result;
	}

	void EventPresentationEditorGraph::RecomputeNextIds() {
		mNextNodeId = 1;
		for (const EventPresentationNodeData& node : nodes) {
			mNextNodeId = std::max(mNextNodeId, node.id + 1);
		}

		mNextPinId = 1;
		for (const EventPresentationPinData& pin : pins) {
			mNextPinId = std::max(mNextPinId, pin.id + 1);
		}

		mNextLinkId = 1;
		for (const EventPresentationLinkData& link : links) {
			mNextLinkId = std::max(mNextLinkId, link.id + 1);
		}
	}
}
