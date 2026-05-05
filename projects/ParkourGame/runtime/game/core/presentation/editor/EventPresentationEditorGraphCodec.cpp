#include "EventPresentationEditorGraphCodec.h"

#include <algorithm>
#include <unordered_set>

#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "game/core/presentation/EventPresentationExecutor.h"

namespace Unnamed {
	namespace {
		constexpr float kTriggerNodeX         = 80.0f;
		constexpr float kConditionSourceNodeX = 360.0f;
		constexpr float kTransformNodeX       = 620.0f;
		constexpr float kActionNodeX          = 900.0f;
		constexpr float kTriggerSpacingY      = 280.0f;
		constexpr float kActionSpacingY       = 170.0f;

		void SetError(std::string* outError, const std::string& message) {
			if (outError) {
				*outError = message;
			}
		}

		[[nodiscard]] const char* NodeKindToToken(
			const EventPresentationNodeKind kind
		) {
			switch (kind) {
				case EventPresentationNodeKind::Trigger: return "trigger";
				case EventPresentationNodeKind::ValueSource: return "value_source";
				case EventPresentationNodeKind::ValueTransform:
					return "value_transform";
				case EventPresentationNodeKind::Action: return "action";
				case EventPresentationNodeKind::Comment: return "comment";
				case EventPresentationNodeKind::Reroute: return "reroute";
				default: return "trigger";
			}
		}

		[[nodiscard]] bool TryParseNodeKind(
			const std::string&           token,
			EventPresentationNodeKind* outKind
		) {
			if (!outKind) {
				return false;
			}
			if (token == "trigger") {
				*outKind = EventPresentationNodeKind::Trigger;
				return true;
			}
			if (token == "value_source") {
				*outKind = EventPresentationNodeKind::ValueSource;
				return true;
			}
			if (token == "value_transform") {
				*outKind = EventPresentationNodeKind::ValueTransform;
				return true;
			}
			if (token == "action") {
				*outKind = EventPresentationNodeKind::Action;
				return true;
			}
			if (token == "comment") {
				*outKind = EventPresentationNodeKind::Comment;
				return true;
			}
			if (token == "reroute") {
				*outKind = EventPresentationNodeKind::Reroute;
				return true;
			}
			return false;
		}

		[[nodiscard]] const char* PinDirectionToToken(
			const EventPresentationPinDirection direction
		) {
			return direction == EventPresentationPinDirection::Output ? "output" :
			                                                            "input";
		}

		[[nodiscard]] bool TryParsePinDirection(
			const std::string&               token,
			EventPresentationPinDirection* outDirection
		) {
			if (!outDirection) {
				return false;
			}
			if (token == "input") {
				*outDirection = EventPresentationPinDirection::Input;
				return true;
			}
			if (token == "output") {
				*outDirection = EventPresentationPinDirection::Output;
				return true;
			}
			return false;
		}

		[[nodiscard]] const char* ValueTypeToToken(
			const EventPresentationValueType valueType
		) {
			switch (valueType) {
				case EventPresentationValueType::Exec: return "exec";
				case EventPresentationValueType::Float: return "float";
				case EventPresentationValueType::Bool: return "bool";
				case EventPresentationValueType::Vec2: return "vec2";
				case EventPresentationValueType::Vec3: return "vec3";
				case EventPresentationValueType::EntityId: return "entity_id";
				default: return "exec";
			}
		}

		[[nodiscard]] bool TryParseValueType(
			const std::string&            token,
			EventPresentationValueType* outValueType
		) {
			if (!outValueType) {
				return false;
			}
			if (token == "exec") {
				*outValueType = EventPresentationValueType::Exec;
				return true;
			}
			if (token == "float") {
				*outValueType = EventPresentationValueType::Float;
				return true;
			}
			if (token == "bool") {
				*outValueType = EventPresentationValueType::Bool;
				return true;
			}
			if (token == "vec2") {
				*outValueType = EventPresentationValueType::Vec2;
				return true;
			}
			if (token == "vec3") {
				*outValueType = EventPresentationValueType::Vec3;
				return true;
			}
			if (token == "entity_id") {
				*outValueType = EventPresentationValueType::EntityId;
				return true;
			}
			return false;
		}

		[[nodiscard]] const char* ValueSourceToToken(
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

		[[nodiscard]] EventPresentationValueSource ParseValueSourceToken(
			const std::string& token
		) {
			if (token == "cue.value2") {
				return EventPresentationValueSource::CueValue2;
			}
			if (token == "payload.xxx") {
				return EventPresentationValueSource::PayloadFloat;
			}
			if (token == "constant") {
				return EventPresentationValueSource::Constant;
			}
			return EventPresentationValueSource::CueValue;
		}

		[[nodiscard]] bool TryBuildGraphFromEditorMetadata(
			const std::string&         sourcePath,
			EventPresentationEditorGraph& outGraph,
			std::string*                outError
		) {
			if (outError) {
				outError->clear();
			}
			if (sourcePath.empty()) {
				return false;
			}

			const JsonReader root(sourcePath);
			if (!root.Valid() || !root.IsObject()) {
				SetError(outError, "Source JSON is invalid or root is not an object.");
				return false;
			}
			const JsonReader editorNode = root["editor"];
			const JsonReader graphNode  = editorNode["graph"];
			if (!graphNode.Valid() || !graphNode.IsObject()) {
				return false;
			}

			EventPresentationEditorGraph metadataGraph = {};
			metadataGraph.Clear();
			metadataGraph.name = root.Has("name") ? root["name"].GetString("") : "";

			const JsonReader viewNode = graphNode["view"];
			if (viewNode.Valid() && viewNode.IsObject()) {
				metadataGraph.viewPan = Vec2(
					viewNode.Has("panX") ? viewNode["panX"].GetFloat(0.0f) : 0.0f,
					viewNode.Has("panY") ? viewNode["panY"].GetFloat(0.0f) : 0.0f
				);
				metadataGraph.viewZoom =
					viewNode.Has("zoom") ? viewNode["zoom"].GetFloat(1.0f) : 1.0f;
			}

			const JsonReader nodesNode = graphNode["nodes"];
			if (!nodesNode.Valid() || !nodesNode.IsArray() || nodesNode.Size() == 0) {
				SetError(outError, "Editor graph nodes array is missing or empty.");
				return false;
			}
			for (size_t i = 0; i < nodesNode.Size(); ++i) {
				const JsonReader nodeReader = nodesNode[i];
				if (!nodeReader.Valid() || !nodeReader.IsObject()) {
					continue;
				}
				const std::optional<uint64_t> nodeId = nodeReader["id"].TryGetUint64();
				if (!nodeId.has_value() || *nodeId == 0) {
					continue;
				}

				EventPresentationNodeKind nodeKind =
					EventPresentationNodeKind::Trigger;
				if (
					!TryParseNodeKind(
						nodeReader["kind"].GetString("trigger"), &nodeKind
					)
				) {
					continue;
				}

				EventPresentationNodeData node = {};
				node.id   = *nodeId;
				node.kind = nodeKind;
				node.title = nodeReader.Has("title") ?
					              nodeReader["title"].GetString("") :
					              std::string(NodeKindToToken(nodeKind));
				node.position = Vec2(
					nodeReader.Has("x") ? nodeReader["x"].GetFloat(0.0f) : 0.0f,
					nodeReader.Has("y") ? nodeReader["y"].GetFloat(0.0f) : 0.0f
				);
				node.execInPinId = nodeReader.Has("execInPinId") ?
					                   nodeReader["execInPinId"].GetUint64() :
					                   0;
				node.execOutPinId = nodeReader.Has("execOutPinId") ?
					                    nodeReader["execOutPinId"].GetUint64() :
					                    0;
				node.floatInPinId = nodeReader.Has("floatInPinId") ?
					                    nodeReader["floatInPinId"].GetUint64() :
					                    0;
				node.floatOutPinId = nodeReader.Has("floatOutPinId") ?
					                     nodeReader["floatOutPinId"].GetUint64() :
					                     0;

				node.cueId = nodeReader.Has("cueId") ?
					             nodeReader["cueId"].GetString("") :
					             "";
				node.cooldownSec = nodeReader.Has("cooldownSec") ?
					                   nodeReader["cooldownSec"].GetFloat(0.0f) :
					                   0.0f;
				node.conditionEnabled = nodeReader.Has("conditionEnabled") ?
					                        nodeReader["conditionEnabled"].GetBool(false) :
					                        false;
				node.conditionMin = nodeReader.Has("conditionMin") ?
					                    nodeReader["conditionMin"].GetFloat(-FLT_MAX) :
					                    -FLT_MAX;
				node.conditionMax = nodeReader.Has("conditionMax") ?
					                    nodeReader["conditionMax"].GetFloat(FLT_MAX) :
					                    FLT_MAX;

				node.valueSource = ParseValueSourceToken(
					nodeReader["valueSource"].GetString("cue.value")
				);
				node.payloadName = nodeReader.Has("payloadName") ?
					                   nodeReader["payloadName"].GetString("") :
					                   "";
				node.constant = nodeReader.Has("constant") ?
					                nodeReader["constant"].GetFloat(0.0f) :
					                0.0f;

				node.clampEnabled = nodeReader.Has("clampEnabled") ?
					                    nodeReader["clampEnabled"].GetBool(false) :
					                    false;
				node.clampMin = nodeReader.Has("clampMin") ?
					                nodeReader["clampMin"].GetFloat(-FLT_MAX) :
					                -FLT_MAX;
				node.clampMax = nodeReader.Has("clampMax") ?
					                nodeReader["clampMax"].GetFloat(FLT_MAX) :
					                FLT_MAX;
				node.multiply = nodeReader.Has("multiply") ?
					                nodeReader["multiply"].GetFloat(1.0f) :
					                1.0f;

				node.actionType = nodeReader.Has("actionType") ?
					                  nodeReader["actionType"].GetString("") :
					                  "";
				node.actionId = nodeReader.Has("actionId") ?
					                nodeReader["actionId"].GetString("") :
					                "";
				node.debugText = nodeReader.Has("debugText") ?
					                 nodeReader["debugText"].GetString("") :
					                 "";
				node.commentText = nodeReader.Has("commentText") ?
					                   nodeReader["commentText"].GetString("") :
					                   "";
				metadataGraph.nodes.emplace_back(std::move(node));
			}
			if (metadataGraph.nodes.empty()) {
				SetError(outError, "Editor graph nodes are empty after parse.");
				return false;
			}

			const JsonReader pinsNode = graphNode["pins"];
			if (pinsNode.Valid() && pinsNode.IsArray()) {
				for (size_t i = 0; i < pinsNode.Size(); ++i) {
					const JsonReader pinReader = pinsNode[i];
					if (!pinReader.Valid() || !pinReader.IsObject()) {
						continue;
					}
					const std::optional<uint64_t> pinId =
						pinReader["id"].TryGetUint64();
					if (!pinId.has_value() || *pinId == 0) {
						continue;
					}

					EventPresentationPinDirection direction =
						EventPresentationPinDirection::Input;
					if (
						!TryParsePinDirection(
							pinReader["direction"].GetString("input"), &direction
						)
					) {
						continue;
					}

					EventPresentationValueType valueType =
						EventPresentationValueType::Exec;
					if (
						!TryParseValueType(
							pinReader["valueType"].GetString("exec"), &valueType
						)
					) {
						continue;
					}

					EventPresentationPinData pin = {};
					pin.id        = *pinId;
					pin.nodeId    = pinReader["nodeId"].GetUint64();
					pin.name      = pinReader.Has("name") ?
						               pinReader["name"].GetString("") :
						               "";
					pin.direction = direction;
					pin.valueType = valueType;
					if (pin.nodeId != 0) {
						metadataGraph.pins.emplace_back(std::move(pin));
					}
				}
			}

			const JsonReader linksNode = graphNode["links"];
			if (linksNode.Valid() && linksNode.IsArray()) {
				for (size_t i = 0; i < linksNode.Size(); ++i) {
					const JsonReader linkReader = linksNode[i];
					if (!linkReader.Valid() || !linkReader.IsObject()) {
						continue;
					}
					const std::optional<uint64_t> linkId =
						linkReader["id"].TryGetUint64();
					if (!linkId.has_value() || *linkId == 0) {
						continue;
					}
					EventPresentationLinkData link = {};
					link.id = *linkId;
					link.fromPinId = linkReader["fromPinId"].GetUint64();
					link.toPinId   = linkReader["toPinId"].GetUint64();
					if (link.fromPinId != 0 && link.toPinId != 0) {
						metadataGraph.links.emplace_back(std::move(link));
					}
				}
			}

			metadataGraph.RecomputeNextIds();
			outGraph = std::move(metadataGraph);
			return true;
		}

		void WriteEditorGraphMetadata(
			JsonWriter&                       writer,
			const EventPresentationEditorGraph& graph
		) {
			writer.Key("editor");
			writer.BeginObject();
			writer.Key("graph");
			writer.BeginObject();

			writer.Key("view");
			writer.BeginObject();
			writer.Key("panX");
			writer.Write(graph.viewPan.x);
			writer.Key("panY");
			writer.Write(graph.viewPan.y);
			writer.Key("zoom");
			writer.Write(graph.viewZoom);
			writer.EndObject();

			writer.Key("nodes");
			writer.BeginArray();
			for (const EventPresentationNodeData& node : graph.nodes) {
				writer.BeginObject();
				writer.Key("id");
				writer.Write(node.id);
				writer.Key("kind");
				writer.Write(std::string(NodeKindToToken(node.kind)));
				writer.Key("title");
				writer.Write(node.title);
				writer.Key("x");
				writer.Write(node.position.x);
				writer.Key("y");
				writer.Write(node.position.y);
				writer.Key("execInPinId");
				writer.Write(node.execInPinId);
				writer.Key("execOutPinId");
				writer.Write(node.execOutPinId);
				writer.Key("floatInPinId");
				writer.Write(node.floatInPinId);
				writer.Key("floatOutPinId");
				writer.Write(node.floatOutPinId);
				writer.Key("cueId");
				writer.Write(node.cueId);
				writer.Key("cooldownSec");
				writer.Write(node.cooldownSec);
				writer.Key("conditionEnabled");
				writer.Write(node.conditionEnabled);
				writer.Key("conditionMin");
				writer.Write(node.conditionMin);
				writer.Key("conditionMax");
				writer.Write(node.conditionMax);
				writer.Key("valueSource");
				writer.Write(std::string(ValueSourceToToken(node.valueSource)));
				writer.Key("payloadName");
				writer.Write(node.payloadName);
				writer.Key("constant");
				writer.Write(node.constant);
				writer.Key("clampEnabled");
				writer.Write(node.clampEnabled);
				writer.Key("clampMin");
				writer.Write(node.clampMin);
				writer.Key("clampMax");
				writer.Write(node.clampMax);
				writer.Key("multiply");
				writer.Write(node.multiply);
				writer.Key("actionType");
				writer.Write(node.actionType);
				writer.Key("actionId");
				writer.Write(node.actionId);
				writer.Key("debugText");
				writer.Write(node.debugText);
				writer.Key("commentText");
				writer.Write(node.commentText);
				writer.EndObject();
			}
			writer.EndArray();

			writer.Key("pins");
			writer.BeginArray();
			for (const EventPresentationPinData& pin : graph.pins) {
				writer.BeginObject();
				writer.Key("id");
				writer.Write(pin.id);
				writer.Key("nodeId");
				writer.Write(pin.nodeId);
				writer.Key("name");
				writer.Write(pin.name);
				writer.Key("direction");
				writer.Write(std::string(PinDirectionToToken(pin.direction)));
				writer.Key("valueType");
				writer.Write(std::string(ValueTypeToToken(pin.valueType)));
				writer.EndObject();
			}
			writer.EndArray();

			writer.Key("links");
			writer.BeginArray();
			for (const EventPresentationLinkData& link : graph.links) {
				writer.BeginObject();
				writer.Key("id");
				writer.Write(link.id);
				writer.Key("fromPinId");
				writer.Write(link.fromPinId);
				writer.Key("toPinId");
				writer.Write(link.toPinId);
				writer.EndObject();
			}
			writer.EndArray();

			writer.EndObject();
			writer.EndObject();
		}

		[[nodiscard]] EventPresentationNodeData* AddTriggerNode(
			EventPresentationEditorGraph& graph,
			const Vec2&                   position
		) {
			const EventPresentationNodeId nodeId = graph.AddNode(
				EventPresentationNodeKind::Trigger,
				"Trigger",
				position
			);
			EventPresentationNodeData* node = graph.FindNode(nodeId);
			if (!node) {
				return nullptr;
			}
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
			return node;
		}

		[[nodiscard]] EventPresentationNodeData* AddActionNode(
			EventPresentationEditorGraph& graph,
			const Vec2&                   position
		) {
			const EventPresentationNodeId nodeId = graph.AddNode(
				EventPresentationNodeKind::Action,
				"Action",
				position
			);
			EventPresentationNodeData* node = graph.FindNode(nodeId);
			if (!node) {
				return nullptr;
			}
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
				"ValueIn",
				EventPresentationPinDirection::Input,
				EventPresentationValueType::Float
			);
			return node;
		}

		[[nodiscard]] EventPresentationNodeData* AddValueSourceNode(
			EventPresentationEditorGraph& graph,
			const Vec2&                   position
		) {
			const EventPresentationNodeId nodeId = graph.AddNode(
				EventPresentationNodeKind::ValueSource,
				"ValueSource",
				position
			);
			EventPresentationNodeData* node = graph.FindNode(nodeId);
			if (!node) {
				return nullptr;
			}
			node->floatOutPinId = graph.AddPin(
				nodeId,
				"FloatOut",
				EventPresentationPinDirection::Output,
				EventPresentationValueType::Float
			);
			return node;
		}

		[[nodiscard]] EventPresentationNodeData* AddValueTransformNode(
			EventPresentationEditorGraph& graph,
			const Vec2&                   position
		) {
			const EventPresentationNodeId nodeId = graph.AddNode(
				EventPresentationNodeKind::ValueTransform,
				"ValueTransform",
				position
			);
			EventPresentationNodeData* node = graph.FindNode(nodeId);
			if (!node) {
				return nullptr;
			}
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
			return node;
		}

		void ApplySourceTextToNode(
			const std::string&         sourceText,
			const float                constantValue,
			EventPresentationNodeData& node
		) {
			if (node.id == 0) {
				Warning(
					"EventPresentationV2",
					"[GraphCodec] ApplySourceTextToNode skipped: invalid node id."
				);
				return;
			}
			std::string payloadName;
			node.valueSource = EventPresentationExecutor::ParseValueSource(
				sourceText,
				&payloadName
			);
			// NOTE:
			// payloadName 側ではなく node 側の参照破損がクラッシュ原因になりやすいため、
			// ここでは move ではなく通常代入を使って切り分け・安定運用します。
			node.payloadName = payloadName;
			node.constant    = constantValue;
		}

		[[nodiscard]] std::string BuildSourceText(
			const EventPresentationNodeData& node
		) {
			switch (node.valueSource) {
				case EventPresentationValueSource::CueValue:
					return "cue.value";
				case EventPresentationValueSource::CueValue2:
					return "cue.value2";
				case EventPresentationValueSource::PayloadFloat:
					return node.payloadName.empty() ?
						       std::string("cue.value") :
						       std::string("payload.") + node.payloadName;
				case EventPresentationValueSource::Constant:
					return "constant";
				default: return "cue.value";
			}
		}

		[[nodiscard]] const EventPresentationLinkData* FindIncomingLink(
			const EventPresentationEditorGraph& graph,
			const EventPresentationPinId        pinId
		) {
			const auto found = std::ranges::find_if(
				graph.links,
				[pinId](const EventPresentationLinkData& link) {
					return link.toPinId == pinId;
				}
			);
			return found == graph.links.end() ? nullptr : &(*found);
		}

		[[nodiscard]] const EventPresentationNodeData* ResolveFloatProducerNode(
			const EventPresentationEditorGraph& graph,
			const EventPresentationPinId        inputPinId,
			std::unordered_set<EventPresentationPinId>& visitedPins
		) {
			if (!visitedPins.emplace(inputPinId).second) {
				return nullptr;
			}
			const EventPresentationLinkData* link = FindIncomingLink(
				graph, inputPinId
			);
			if (!link) {
				return nullptr;
			}
			const EventPresentationPinData* fromPin = graph.FindPin(link->fromPinId);
			if (!fromPin) {
				return nullptr;
			}
			const EventPresentationNodeData* node = graph.FindNode(fromPin->nodeId);
			if (!node) {
				return nullptr;
			}
			if (node->kind == EventPresentationNodeKind::Reroute &&
			    node->floatInPinId != 0) {
				return ResolveFloatProducerNode(
					graph, node->floatInPinId, visitedPins
				);
			}
			return node;
		}

		bool FillValueInputFromGraph(
			const EventPresentationEditorGraph& graph,
			const EventPresentationPinId        actionValueInputPinId,
			EventPresentationValueInputAssetData& outValueInput,
			std::string*                          outError
		) {
			outValueInput = {};
			outValueInput.source = "cue.value";

			std::unordered_set<EventPresentationPinId> visitedPins;
			const EventPresentationNodeData* producer = ResolveFloatProducerNode(
				graph,
				actionValueInputPinId,
				visitedPins
			);
			if (!producer) {
				return true;
			}

			const EventPresentationNodeData* sourceNode = nullptr;
			if (producer->kind == EventPresentationNodeKind::ValueTransform) {
				outValueInput.clampEnabled = producer->clampEnabled;
				outValueInput.clampMin     = producer->clampMin;
				outValueInput.clampMax     = producer->clampMax;
				outValueInput.multiply     = producer->multiply;

				std::unordered_set<EventPresentationPinId> nestedVisited;
				sourceNode = ResolveFloatProducerNode(
					graph,
					producer->floatInPinId,
					nestedVisited
				);
			} else {
				sourceNode = producer;
			}

			if (!sourceNode) {
				return true;
			}
			if (sourceNode->kind != EventPresentationNodeKind::ValueSource) {
				SetError(outError, "Unsupported value input upstream node.");
				return false;
			}

			outValueInput.source = BuildSourceText(*sourceNode);
			if (sourceNode->valueSource == EventPresentationValueSource::Constant) {
				outValueInput.constant = sourceNode->constant;
			}
			return true;
		}

		[[nodiscard]] std::vector<const EventPresentationNodeData*>
		CollectTriggerNodes(const EventPresentationEditorGraph& graph) {
			std::vector<const EventPresentationNodeData*> triggers;
			for (const EventPresentationNodeData& node : graph.nodes) {
				if (node.kind == EventPresentationNodeKind::Trigger) {
					triggers.emplace_back(&node);
				}
			}
			std::ranges::sort(
				triggers,
				[](const EventPresentationNodeData* lhs,
				   const EventPresentationNodeData* rhs) {
					if (lhs->position.y != rhs->position.y) {
						return lhs->position.y < rhs->position.y;
					}
					return lhs->position.x < rhs->position.x;
				}
			);
			return triggers;
		}

		[[nodiscard]] const EventPresentationNodeData* FindNextActionNode(
			const EventPresentationEditorGraph& graph,
			const EventPresentationPinId        execOutPinId,
			bool*                               outAmbiguous = nullptr
		) {
			const EventPresentationLinkData* matchedLink = nullptr;
			if (outAmbiguous) {
				*outAmbiguous = false;
			}
			for (const EventPresentationLinkData& link : graph.links) {
				if (link.fromPinId != execOutPinId) {
					continue;
				}
				const EventPresentationPinData* toPin = graph.FindPin(link.toPinId);
				if (!toPin) {
					continue;
				}
				const EventPresentationNodeData* toNode = graph.FindNode(toPin->nodeId);
				if (
					!toNode || toNode->kind != EventPresentationNodeKind::Action ||
					toNode->execInPinId != toPin->id
				) {
					continue;
				}
				if (matchedLink) {
					if (outAmbiguous) {
						*outAmbiguous = true;
					}
					return nullptr;
				}
				matchedLink = &link;
			}
			if (!matchedLink) {
				return nullptr;
			}
			const EventPresentationPinData* toPin = graph.FindPin(matchedLink->toPinId);
			return toPin ? graph.FindNode(toPin->nodeId) : nullptr;
		}
	}

	bool EventPresentationEditorGraphCodec::BuildGraphFromAsset(
		const EventPresentationAssetData& assetData,
		EventPresentationEditorGraph&     outGraph,
		const std::string&                sourcePath,
		std::string*                      outError
	) {
		if (!sourcePath.empty()) {
			std::string editorMetadataError;
			if (TryBuildGraphFromEditorMetadata(
				    sourcePath, outGraph, &editorMetadataError
			    )) {
				if (outGraph.name.empty()) {
					outGraph.name = assetData.name;
				}
				return true;
			}
#ifdef _DEBUG
			if (!editorMetadataError.empty()) {
				DevMsg(
					"EventPresentationV2",
					"[GraphCodec] editor metadata parse failed, fallback to runtime graph build: {}",
					editorMetadataError
				);
			}
#endif
		}

		outGraph.Clear();
		outGraph.name = assetData.name;

		// 変換中の再配置頻度を下げるため、概算で先に reserve します。
		size_t estimatedNodeCount = 0;
		size_t estimatedPinCount  = 0;
		size_t estimatedLinkCount = 0;
		for (const EventPresentationTriggerAssetData& triggerData : assetData.
		     triggers) {
			const size_t actionCount = triggerData.actions.size();
			estimatedNodeCount += 1 + actionCount * 3;
			estimatedPinCount += 2 + actionCount * 6;
			estimatedLinkCount += actionCount * 3;
			if (triggerData.condition.enabled) {
				estimatedNodeCount += 2;
				estimatedPinCount += 3;
				estimatedLinkCount += 2;
			}
		}
		outGraph.nodes.reserve(estimatedNodeCount);
		outGraph.pins.reserve(estimatedPinCount);
		outGraph.links.reserve(estimatedLinkCount);

#ifdef _DEBUG
		DevMsg(
			"EventPresentationV2",
			"[GraphCodec] BuildGraphFromAsset begin: triggers={} estNodes={} estPins={} estLinks={}",
			static_cast<int>(assetData.triggers.size()),
			static_cast<unsigned long long>(estimatedNodeCount),
			static_cast<unsigned long long>(estimatedPinCount),
			static_cast<unsigned long long>(estimatedLinkCount)
		);
#endif

		float triggerY = 80.0f;
		for (const EventPresentationTriggerAssetData& triggerData : assetData.
		     triggers) {
			EventPresentationNodeData* triggerNode = AddTriggerNode(
				outGraph,
				Vec2(kTriggerNodeX, triggerY)
			);
			if (!triggerNode) {
				SetError(outError, "Failed to create trigger node.");
				return false;
			}
			[[maybe_unused]] const EventPresentationNodeId triggerNodeId =
				triggerNode->id;
			triggerNode->cueId            = triggerData.cueId;
			triggerNode->cooldownSec      = triggerData.cooldownSec;
			triggerNode->conditionEnabled = triggerData.condition.enabled;
			triggerNode->conditionMin     = triggerData.condition.minValue;
			triggerNode->conditionMax     = triggerData.condition.maxValue;
			const EventPresentationPinId triggerExecOutPinId =
				triggerNode->execOutPinId;
			const EventPresentationPinId triggerConditionInPinId =
				triggerNode->floatInPinId;

			if (triggerData.condition.enabled) {
				EventPresentationNodeData* conditionSourceNode = AddValueSourceNode(
					outGraph,
					Vec2(kConditionSourceNodeX, triggerY - 20.0f)
				);
				if (!conditionSourceNode) {
					SetError(outError, "Failed to create condition nodes.");
					return false;
				}
				[[maybe_unused]] const EventPresentationNodeId
					conditionSourceNodeId = conditionSourceNode->id;
				ApplySourceTextToNode(
					triggerData.condition.source,
					0.0f,
					*conditionSourceNode
				);
				const EventPresentationPinId conditionSourceOutPinId =
					conditionSourceNode->floatOutPinId;

				EventPresentationNodeData* conditionTransformNode =
					AddValueTransformNode(
						outGraph,
						Vec2(kTransformNodeX, triggerY - 20.0f)
					);
				if (!conditionTransformNode) {
					SetError(outError, "Failed to create condition transform node.");
					return false;
				}
				[[maybe_unused]] const EventPresentationNodeId
					conditionTransformNodeId = conditionTransformNode->id;
				conditionTransformNode->clampEnabled = false;
				conditionTransformNode->clampMin     = -FLT_MAX;
				conditionTransformNode->clampMax     = FLT_MAX;
				conditionTransformNode->multiply     = 1.0f;
				const EventPresentationPinId conditionTransformInPinId =
					conditionTransformNode->floatInPinId;
				const EventPresentationPinId conditionTransformOutPinId =
					conditionTransformNode->floatOutPinId;

				(void)outGraph.AddLink(
					conditionSourceOutPinId,
					conditionTransformInPinId
				);
				(void)outGraph.AddLink(
					conditionTransformOutPinId,
					triggerConditionInPinId
				);

#ifdef _DEBUG
				DevMsg(
					"EventPresentationV2",
					"[GraphCodec] condition pipeline: triggerNode={} sourceNode={} transformNode={}",
					triggerNodeId,
					conditionSourceNodeId,
					conditionTransformNodeId
				);
#endif
			}

			EventPresentationPinId prevExecPinId = triggerExecOutPinId;
			float                  actionY       = triggerY;
			for (const EventPresentationActionAssetData& actionData : triggerData.
			     actions) {
				EventPresentationNodeData* actionNode = AddActionNode(
					outGraph,
					Vec2(kActionNodeX, actionY)
				);
				if (!actionNode) {
					SetError(outError, "Failed to create action pipeline nodes.");
					return false;
				}
				[[maybe_unused]] const EventPresentationNodeId actionNodeId =
					actionNode->id;

				actionNode->actionType = actionData.type;
				actionNode->actionId   = actionData.id;
				actionNode->debugText  = actionData.debugText;
				const EventPresentationPinId actionExecInPinId =
					actionNode->execInPinId;
				const EventPresentationPinId actionExecOutPinId =
					actionNode->execOutPinId;
				const EventPresentationPinId actionFloatInPinId =
					actionNode->floatInPinId;

				EventPresentationNodeData* sourceNode = AddValueSourceNode(
					outGraph,
					Vec2(kConditionSourceNodeX, actionY + 45.0f)
				);
				if (!sourceNode) {
					SetError(outError, "Failed to create action value source node.");
					return false;
				}
				[[maybe_unused]] const EventPresentationNodeId sourceNodeId =
					sourceNode->id;
				const EventPresentationPinId  sourceOutPinId =
					sourceNode->floatOutPinId;

				ApplySourceTextToNode(
					actionData.valueInput.source,
					actionData.valueInput.constant,
					*sourceNode
				);

				EventPresentationNodeData* transformNode = AddValueTransformNode(
					outGraph,
					Vec2(kTransformNodeX, actionY + 45.0f)
				);
				if (!transformNode) {
					SetError(outError, "Failed to create action transform node.");
					return false;
				}
				[[maybe_unused]] const EventPresentationNodeId transformNodeId =
					transformNode->id;
				const EventPresentationPinId  transformInPinId =
					transformNode->floatInPinId;
				const EventPresentationPinId transformOutPinId =
					transformNode->floatOutPinId;
				transformNode->clampEnabled = actionData.valueInput.clampEnabled;
				transformNode->clampMin     = actionData.valueInput.clampMin;
				transformNode->clampMax     = actionData.valueInput.clampMax;
				transformNode->multiply     = actionData.valueInput.multiply;

				(void)outGraph.AddLink(prevExecPinId, actionExecInPinId);
				(void)outGraph.AddLink(sourceOutPinId, transformInPinId);
				(void)outGraph.AddLink(transformOutPinId, actionFloatInPinId);

				prevExecPinId = actionExecOutPinId;
				actionY += kActionSpacingY;

#ifdef _DEBUG
				DevMsg(
					"EventPresentationV2",
					"[GraphCodec] action pipeline: triggerNode={} actionNode={} sourceNode={} transformNode={} actionType='{}'",
					triggerNodeId,
					actionNodeId,
					sourceNodeId,
					transformNodeId,
					actionData.type
				);
#endif
			}

			triggerY = std::max(triggerY + kTriggerSpacingY, actionY + 80.0f);
		}

		return true;
	}

	bool EventPresentationEditorGraphCodec::BuildAssetFromGraph(
		const EventPresentationEditorGraph& graph,
		EventPresentationAssetData&          outAssetData,
		std::string*                         outError
	) {
		outAssetData = {};
		outAssetData.name = graph.name;

		for (const EventPresentationNodeData& node : graph.nodes) {
			if (node.kind == EventPresentationNodeKind::Comment) {
				continue;
			}
			if (node.kind == EventPresentationNodeKind::Reroute) {
				SetError(
					outError,
					"Reroute node export is not supported yet."
				);
				return false;
			}
		}

		const std::vector<const EventPresentationNodeData*> triggerNodes =
			CollectTriggerNodes(graph);
		if (triggerNodes.empty()) {
			SetError(outError, "Graph has no trigger node.");
			return false;
		}

		for (const EventPresentationNodeData* triggerNode : triggerNodes) {
			if (!triggerNode) {
				continue;
			}
			EventPresentationTriggerAssetData triggerData = {};
			triggerData.cueId                             = triggerNode->cueId;
			triggerData.cooldownSec = std::max(0.0f, triggerNode->cooldownSec);
			triggerData.condition.enabled = triggerNode->conditionEnabled;
			triggerData.condition.minValue = triggerNode->conditionMin;
			triggerData.condition.maxValue = triggerNode->conditionMax;
			if (triggerData.condition.maxValue < triggerData.condition.minValue) {
				std::swap(
					triggerData.condition.minValue,
					triggerData.condition.maxValue
				);
			}

			if (triggerNode->conditionEnabled && triggerNode->floatInPinId != 0) {
				EventPresentationValueInputAssetData conditionInput = {};
				if (!FillValueInputFromGraph(
					    graph,
					    triggerNode->floatInPinId,
					    conditionInput,
					    outError
				    )) {
					return false;
				}
				triggerData.condition.source = conditionInput.source;
			}

			EventPresentationPinId                    execCursorPinId =
				triggerNode->execOutPinId;
			std::unordered_set<EventPresentationNodeId> visitedActions;
			while (execCursorPinId != 0) {
				bool ambiguousExecLink = false;
				const EventPresentationNodeData* actionNode = FindNextActionNode(
					graph,
					execCursorPinId,
					&ambiguousExecLink
				);
				if (ambiguousExecLink) {
					SetError(
						outError,
						"Trigger exec chain has multiple outgoing links from a single exec output pin."
					);
					return false;
				}
				if (!actionNode) {
					break;
				}
				if (!visitedActions.emplace(actionNode->id).second) {
					SetError(outError, "Action exec chain contains a cycle.");
					return false;
				}

				EventPresentationActionAssetData actionData = {};
				actionData.type                             = actionNode->actionType;
				actionData.id                               = actionNode->actionId;
				actionData.debugText                        = actionNode->debugText;

				if (!FillValueInputFromGraph(
					    graph,
					    actionNode->floatInPinId,
					    actionData.valueInput,
					    outError
				    )) {
					return false;
				}

				triggerData.actions.emplace_back(std::move(actionData));
				execCursorPinId = actionNode->execOutPinId;
			}

			if (triggerData.actions.empty()) {
				SetError(outError, "Trigger has no action chain.");
				return false;
			}
			outAssetData.triggers.emplace_back(std::move(triggerData));
		}

		return true;
	}

	bool EventPresentationEditorGraphCodec::SaveAssetJson(
		const EventPresentationAssetData& assetData,
		const EventPresentationEditorGraph* editorGraph,
		const std::string&                path,
		std::string*                      outError
	) {
		try {
			JsonWriter writer(path);
			writer.BeginObject();
			writer.Key("name");
			writer.Write(assetData.name);

			writer.Key("triggers");
			writer.BeginArray();
			for (const EventPresentationTriggerAssetData& trigger : assetData.
			     triggers) {
				writer.BeginObject();
				writer.Key("cueId");
				writer.Write(trigger.cueId);
				writer.Key("cooldownSec");
				writer.Write(trigger.cooldownSec);

				if (trigger.condition.enabled) {
					writer.Key("condition");
					writer.BeginObject();
					writer.Key("enabled");
					writer.Write(true);
					writer.Key("source");
					writer.Write(trigger.condition.source);
					writer.Key("min");
					writer.Write(trigger.condition.minValue);
					writer.Key("max");
					writer.Write(trigger.condition.maxValue);
					writer.EndObject();
				}

				writer.Key("actions");
				writer.BeginArray();
				for (const EventPresentationActionAssetData& action : trigger.
				     actions) {
					writer.BeginObject();
					writer.Key("type");
					writer.Write(action.type);
					if (!action.id.empty()) {
						writer.Key("id");
						writer.Write(action.id);
					}
					if (!action.debugText.empty()) {
						writer.Key("debugText");
						writer.Write(action.debugText);
					}

					writer.Key("value");
					writer.BeginObject();
					writer.Key("source");
					writer.Write(action.valueInput.source);
					if (action.valueInput.source == "constant") {
						writer.Key("constant");
						writer.Write(action.valueInput.constant);
					}
					if (action.valueInput.clampEnabled) {
						writer.Key("clampEnabled");
						writer.Write(true);
						writer.Key("clampMin");
						writer.Write(action.valueInput.clampMin);
						writer.Key("clampMax");
						writer.Write(action.valueInput.clampMax);
					}
					if (action.valueInput.multiply != 1.0f) {
						writer.Key("multiply");
						writer.Write(action.valueInput.multiply);
					}
					writer.EndObject();
					writer.EndObject();
				}
				writer.EndArray();
				writer.EndObject();
			}
			writer.EndArray();
			if (editorGraph) {
				WriteEditorGraphMetadata(writer, *editorGraph);
			}
			writer.EndObject();
			return writer.Save();
		} catch (const std::exception& ex) {
			SetError(
				outError,
				std::string("Failed to save EventPresentation JSON: ") + ex.what()
			);
			return false;
		}
	}
}
