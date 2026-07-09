#include "SequenceMigrator.h"

#include <array>

#include "core/guidgenerator/GuidGenerator.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] uint64_t AllocateStableId() {
			static GuidGenerator generator;
			return generator.Alloc();
		}

		[[nodiscard]] bool EnsureIdField(
			nlohmann::json& node,
			const char*     keyName
		) {
			if (!node.is_object()) {
				return false;
			}
			const auto it = node.find(keyName);
			if (it != node.end() && it->is_number_unsigned()) {
				return false;
			}
			if (it != node.end() && it->is_number_integer() && it->get<
				    int64_t>() > 0) {
				return false;
			}
			node[keyName] = AllocateStableId();
			return true;
		}

		bool EnsureCurveNodeIds(nlohmann::json& curveNode) {
			if (!curveNode.is_object()) {
				return false;
			}

			bool changed = false;
			changed      |= EnsureIdField(curveNode, "channelId");
			if (!curveNode.contains("keys") || !curveNode["keys"].is_array()) {
				curveNode["keys"] = nlohmann::json::array();
				changed           = true;
				return changed;
			}
			for (auto& keyNode : curveNode["keys"]) {
				changed |= EnsureIdField(keyNode, "keyId");
			}
			return changed;
		}

		bool EnsureSectionIds(nlohmann::json& sectionNode) {
			if (!sectionNode.is_object()) {
				return false;
			}

			bool changed = false;
			changed      |= EnsureIdField(sectionNode, "sectionId");

			static constexpr std::array kCurveKeys = {
				"floatCurve",
				"vec3XCurve", "vec3YCurve", "vec3ZCurve",
				"transformPosX", "transformPosY", "transformPosZ",
				"transformRotX", "transformRotY", "transformRotZ",
				"transformRotW",
				"transformScaleX", "transformScaleY", "transformScaleZ"
			};
			for (const char* curveKey : kCurveKeys) {
				if (!sectionNode.contains(curveKey) || !sectionNode[curveKey].
				    is_object()) {
					continue;
				}
				changed |= EnsureCurveNodeIds(sectionNode[curveKey]);
			}

			if (sectionNode.contains("boolKeys") && sectionNode["boolKeys"].
			    is_array()) {
				for (auto& keyNode : sectionNode["boolKeys"]) {
					changed |= EnsureIdField(keyNode, "keyId");
				}
			}
			if (sectionNode.contains("cameraCutKeys") && sectionNode[
				    "cameraCutKeys"].is_array()) {
				for (auto& keyNode : sectionNode["cameraCutKeys"]) {
					changed |= EnsureIdField(keyNode, "keyId");
				}
			}
			if (sectionNode.contains("eventKeys") && sectionNode["eventKeys"].
			    is_array()) {
				for (auto& keyNode : sectionNode["eventKeys"]) {
					changed |= EnsureIdField(keyNode, "keyId");
				}
			}

			if (sectionNode.contains("skeletal") && sectionNode["skeletal"].
			    is_object()) {
				auto& skeletalNode = sectionNode["skeletal"];
				for (const char* skeletalCurve : {
					     "weightCurve", "playbackCurve", "speedCurve"
				     }) {
					if (!skeletalNode.contains(skeletalCurve) || !skeletalNode[
						    skeletalCurve].is_object()) {
						continue;
					}
					changed |= EnsureCurveNodeIds(skeletalNode[skeletalCurve]);
				}
			}
			return changed;
		}
	}

	bool SequenceMigrator::MigrateToLatest(
		nlohmann::json& ioRoot,
		bool*           outChanged,
		int32_t*        outSourceVersion
	) {
		if (!ioRoot.is_object()) {
			if (outChanged) {
				*outChanged = false;
			}
			if (outSourceVersion) {
				*outSourceVersion = 0;
			}
			return false;
		}

		const int32_t sourceVersion = ioRoot.value("version", 1);
		if (outSourceVersion) {
			*outSourceVersion = sourceVersion;
		}

		bool changed = false;
		if (sourceVersion < 2) {
			ioRoot["version"] = 2;
			changed           = true;
		}

		if (!ioRoot.contains("bindings") || !ioRoot["bindings"].is_array()) {
			ioRoot["bindings"] = nlohmann::json::array();
			changed            = true;
		}
		for (auto& bindingNode : ioRoot["bindings"]) {
			changed |= EnsureIdField(bindingNode, "bindingId");
		}

		if (!ioRoot.contains("tracks") || !ioRoot["tracks"].is_array()) {
			ioRoot["tracks"] = nlohmann::json::array();
			changed          = true;
		}
		for (auto& trackNode : ioRoot["tracks"]) {
			if (!trackNode.is_object()) {
				continue;
			}
			changed |= EnsureIdField(trackNode, "trackId");
			if (!trackNode.contains("sections") || !trackNode["sections"].
			    is_array()) {
				trackNode["sections"] = nlohmann::json::array();
				changed               = true;
			}
			for (auto& sectionNode : trackNode["sections"]) {
				changed |= EnsureSectionIds(sectionNode);
			}
		}

		if (!ioRoot.contains("editor") || !ioRoot["editor"].is_object()) {
			ioRoot["editor"] = nlohmann::json::object();
			changed          = true;
		}
		nlohmann::json& editorNode = ioRoot["editor"];
		if (!editorNode.contains("scrubFireEvents") || !editorNode[
			    "scrubFireEvents"].is_boolean()) {
			editorNode["scrubFireEvents"] = false;
			changed                       = true;
		}
		if (!editorNode.contains("autoKeyEnabled") || !editorNode[
			    "autoKeyEnabled"].is_boolean()) {
			editorNode["autoKeyEnabled"] = false;
			changed                      = true;
		}

		if (ioRoot.value("version", 1) != 2) {
			ioRoot["version"] = 2;
			changed           = true;
		}

		if (outChanged) {
			*outChanged = changed;
		}
		return true;
	}
}
