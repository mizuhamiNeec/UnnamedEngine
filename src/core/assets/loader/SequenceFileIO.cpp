#include "SequenceFileIO.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <json.hpp>

#include "SequenceMigrator.h"
#include "core/path/PathUtil.h"
#include "core/string/StrUtil.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] SEQUENCE_INTERPOLATION_MODE ParseInterpolation(
			const nlohmann::json& value
		) {
			if (value.is_string()) {
				const std::string text = StrUtil::ToLowerCase(value.get<std::string>());
				if (text == "step") {
					return SEQUENCE_INTERPOLATION_MODE::MODE_STEP;
				}
				if (text == "cubic") {
					return SEQUENCE_INTERPOLATION_MODE::MODE_CUBIC;
				}
				return SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR;
			}
			const int raw = value.is_number() ? value.get<int>() : 1;
			if (raw == 0) {
				return SEQUENCE_INTERPOLATION_MODE::MODE_STEP;
			}
			if (raw == 2) {
				return SEQUENCE_INTERPOLATION_MODE::MODE_CUBIC;
			}
			return SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR;
		}

		[[nodiscard]] nlohmann::json ToJsonInterpolation(
			const SEQUENCE_INTERPOLATION_MODE mode
		) {
			switch (mode) {
				case SEQUENCE_INTERPOLATION_MODE::MODE_STEP: return "step";
				case SEQUENCE_INTERPOLATION_MODE::MODE_CUBIC: return "cubic";
				case SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR:
				default: return "linear";
			}
		}

		[[nodiscard]] SEQUENCE_BLEND_MODE ParseBlendMode(const nlohmann::json& value) {
			if (value.is_string()) {
				return StrUtil::ToLowerCase(value.get<std::string>()) == "additive" ?
					       SEQUENCE_BLEND_MODE::MODE_ADDITIVE :
					       SEQUENCE_BLEND_MODE::MODE_ABSOLUTE;
			}
			return value.is_number_integer() && value.get<int>() == 1 ?
				       SEQUENCE_BLEND_MODE::MODE_ADDITIVE :
				       SEQUENCE_BLEND_MODE::MODE_ABSOLUTE;
		}

		[[nodiscard]] nlohmann::json ToJsonBlendMode(const SEQUENCE_BLEND_MODE mode) {
			return mode == SEQUENCE_BLEND_MODE::MODE_ADDITIVE ? "additive" : "absolute";
		}

		[[nodiscard]] SEQUENCE_TRACK_TYPE ParseTrackType(const nlohmann::json& value) {
			if (value.is_string()) {
				const std::string text = StrUtil::ToLowerCase(value.get<std::string>());
				if (text == "transform") {
					return SEQUENCE_TRACK_TYPE::TRANSFORM;
				}
				if (text == "skeletalcontrol") {
					return SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL;
				}
				if (text == "cameracut") {
					return SEQUENCE_TRACK_TYPE::CAMERA_CUT;
				}
				if (text == "event") {
					return SEQUENCE_TRACK_TYPE::EVENT;
				}
				if (text == "visibility") {
					return SEQUENCE_TRACK_TYPE::VISIBILITY;
				}
				if (text == "activation") {
					return SEQUENCE_TRACK_TYPE::ACTIVATION;
				}
				if (text == "propertybool") {
					return SEQUENCE_TRACK_TYPE::PROPERTY_BOOL;
				}
				if (text == "propertyvec3") {
					return SEQUENCE_TRACK_TYPE::PROPERTY_VEC3;
				}
				return SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT;
			}

			const int raw = value.is_number_integer() ?
				                value.get<int>() :
				                static_cast<int>(SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT);
			if (
				raw < static_cast<int>(SEQUENCE_TRACK_TYPE::TRANSFORM) ||
				raw > static_cast<int>(SEQUENCE_TRACK_TYPE::PROPERTY_VEC3)
			) {
				return SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT;
			}
			return static_cast<SEQUENCE_TRACK_TYPE>(raw);
		}

		[[nodiscard]] nlohmann::json ToJsonTrackType(const SEQUENCE_TRACK_TYPE type) {
			switch (type) {
				case SEQUENCE_TRACK_TYPE::TRANSFORM: return "transform";
				case SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL: return "skeletalControl";
				case SEQUENCE_TRACK_TYPE::CAMERA_CUT: return "cameraCut";
				case SEQUENCE_TRACK_TYPE::EVENT: return "event";
				case SEQUENCE_TRACK_TYPE::VISIBILITY: return "visibility";
				case SEQUENCE_TRACK_TYPE::ACTIVATION: return "activation";
				case SEQUENCE_TRACK_TYPE::PROPERTY_BOOL: return "propertyBool";
				case SEQUENCE_TRACK_TYPE::PROPERTY_VEC3: return "propertyVec3";
				case SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT:
				default: return "propertyFloat";
			}
		}

		void SortFloatKeys(SequenceRichCurveAssetData& curve) {
			std::ranges::sort(
				curve.keys,
				[](const SequenceFloatKeyAssetData& lhs, const SequenceFloatKeyAssetData& rhs) {
					return lhs.frame < rhs.frame;
				}
			);
		}

		void SortBoolKeys(std::vector<SequenceBoolKeyAssetData>& keys) {
			std::ranges::sort(
				keys,
				[](const SequenceBoolKeyAssetData& lhs, const SequenceBoolKeyAssetData& rhs) {
					return lhs.frame < rhs.frame;
				}
			);
		}

		void SortEventKeys(std::vector<SequenceEventKeyAssetData>& keys) {
			std::ranges::sort(
				keys,
				[](const SequenceEventKeyAssetData& lhs, const SequenceEventKeyAssetData& rhs) {
					return lhs.frame < rhs.frame;
				}
			);
		}

		void SortCameraCutKeys(std::vector<SequenceCameraCutKeyAssetData>& keys) {
			std::ranges::sort(
				keys,
				[](const SequenceCameraCutKeyAssetData& lhs, const SequenceCameraCutKeyAssetData& rhs) {
					return lhs.frame < rhs.frame;
				}
			);
		}

		SequenceRichCurveAssetData ParseCurve(const nlohmann::json& node) {
			SequenceRichCurveAssetData curve = {};
			if (!node.is_object()) {
				return curve;
			}
			if (node.contains("channelId") && node["channelId"].is_number_unsigned()) {
				curve.channelId = node["channelId"].get<uint64_t>();
			}
			if (!node.contains("keys") || !node["keys"].is_array()) {
				return curve;
			}
			curve.keys.reserve(node["keys"].size());
			for (const nlohmann::json& keyNode : node["keys"]) {
				if (!keyNode.is_object()) {
					continue;
				}
				SequenceFloatKeyAssetData key = {};
				key.keyId = keyNode.value("keyId", 0ull);
				key.frame = keyNode.value("frame", 0ll);
				key.value = keyNode.value("value", 0.0f);
				key.arriveTangent = keyNode.value("arriveTangent", 0.0f);
				key.leaveTangent = keyNode.value("leaveTangent", 0.0f);
				if (keyNode.contains("interpolation")) {
					key.interpolation = ParseInterpolation(keyNode["interpolation"]);
				}
				curve.keys.emplace_back(key);
			}
			SortFloatKeys(curve);
			return curve;
		}

		nlohmann::json ToJsonCurve(const SequenceRichCurveAssetData& curve) {
			nlohmann::json node = nlohmann::json::object();
			node["channelId"]   = curve.channelId;
			node["keys"]        = nlohmann::json::array();
			for (const SequenceFloatKeyAssetData& key : curve.keys) {
				nlohmann::json keyNode = nlohmann::json::object();
				keyNode["keyId"]         = key.keyId;
				keyNode["frame"]         = key.frame;
				keyNode["value"]         = key.value;
				keyNode["arriveTangent"] = key.arriveTangent;
				keyNode["leaveTangent"]  = key.leaveTangent;
				keyNode["interpolation"] = ToJsonInterpolation(key.interpolation);
				node["keys"].emplace_back(std::move(keyNode));
			}
			return node;
		}

		SequenceSectionAssetData ParseSection(const nlohmann::json& node) {
			SequenceSectionAssetData section = {};
			if (!node.is_object()) {
				return section;
			}
			section.sectionId  = node.value("sectionId", 0ull);
			section.startFrame = node.value("startFrame", 0ll);
			section.endFrame   = node.value("endFrame", section.startFrame);
			section.priority   = node.value("priority", 0);

			section.floatCurve = ParseCurve(node.value("floatCurve", nlohmann::json::object()));
			section.vec3XCurve = ParseCurve(node.value("vec3XCurve", nlohmann::json::object()));
			section.vec3YCurve = ParseCurve(node.value("vec3YCurve", nlohmann::json::object()));
			section.vec3ZCurve = ParseCurve(node.value("vec3ZCurve", nlohmann::json::object()));
			section.transformPosX = ParseCurve(node.value("transformPosX", nlohmann::json::object()));
			section.transformPosY = ParseCurve(node.value("transformPosY", nlohmann::json::object()));
			section.transformPosZ = ParseCurve(node.value("transformPosZ", nlohmann::json::object()));
			section.transformRotX = ParseCurve(node.value("transformRotX", nlohmann::json::object()));
			section.transformRotY = ParseCurve(node.value("transformRotY", nlohmann::json::object()));
			section.transformRotZ = ParseCurve(node.value("transformRotZ", nlohmann::json::object()));
			section.transformRotW = ParseCurve(node.value("transformRotW", nlohmann::json::object()));
			section.transformScaleX = ParseCurve(node.value("transformScaleX", nlohmann::json::object()));
			section.transformScaleY = ParseCurve(node.value("transformScaleY", nlohmann::json::object()));
			section.transformScaleZ = ParseCurve(node.value("transformScaleZ", nlohmann::json::object()));

			if (node.contains("boolKeys") && node["boolKeys"].is_array()) {
				section.boolKeys.reserve(node["boolKeys"].size());
				for (const nlohmann::json& keyNode : node["boolKeys"]) {
					if (!keyNode.is_object()) {
						continue;
					}
					section.boolKeys.emplace_back(
						SequenceBoolKeyAssetData{
							.keyId = keyNode.value("keyId", 0ull),
							.frame = keyNode.value("frame", 0ll),
							.value = keyNode.value("value", false),
						}
					);
				}
				SortBoolKeys(section.boolKeys);
			}

			if (node.contains("cameraCutKeys") && node["cameraCutKeys"].is_array()) {
				section.cameraCutKeys.reserve(node["cameraCutKeys"].size());
				for (const nlohmann::json& keyNode : node["cameraCutKeys"]) {
					if (!keyNode.is_object()) {
						continue;
					}
					section.cameraCutKeys.emplace_back(
						SequenceCameraCutKeyAssetData{
							.keyId            = keyNode.value("keyId", 0ull),
							.frame            = keyNode.value("frame", 0ll),
							.cameraEntityGuid = keyNode.value("cameraEntityGuid", 0ull),
						}
					);
				}
				SortCameraCutKeys(section.cameraCutKeys);
			}

			if (node.contains("eventKeys") && node["eventKeys"].is_array()) {
				section.eventKeys.reserve(node["eventKeys"].size());
				for (const nlohmann::json& keyNode : node["eventKeys"]) {
					if (!keyNode.is_object()) {
						continue;
					}
					SequenceEventKeyAssetData key = {};
					key.keyId           = keyNode.value("keyId", 0ull);
					key.frame           = keyNode.value("frame", 0ll);
					key.cueId           = keyNode.value("cueId", std::string());
					key.sourceEntityGuid = keyNode.value("sourceEntityGuid", 0ull);
					key.cueValue        = keyNode.value("cueValue", 0.0f);
					key.cueValue2       = keyNode.value("cueValue2", 0.0f);
					key.consoleCommand  = keyNode.value("consoleCommand", std::string());
					section.eventKeys.emplace_back(std::move(key));
				}
				SortEventKeys(section.eventKeys);
			}

			if (node.contains("skeletal") && node["skeletal"].is_object()) {
				const auto& skeletalNode = node["skeletal"];
				section.skeletal.stateId = skeletalNode.value("stateId", std::string());
				section.skeletal.layerIndex = skeletalNode.value("layerIndex", 0);
				section.skeletal.playOnEnter = skeletalNode.value("playOnEnter", false);
				section.skeletal.stopOnExit = skeletalNode.value("stopOnExit", false);
				section.skeletal.weightCurve = ParseCurve(skeletalNode.value("weightCurve", nlohmann::json::object()));
				section.skeletal.playbackCurve = ParseCurve(skeletalNode.value("playbackCurve", nlohmann::json::object()));
				section.skeletal.speedCurve = ParseCurve(skeletalNode.value("speedCurve", nlohmann::json::object()));
			}

			return section;
		}

		nlohmann::json ToJsonSection(const SequenceSectionAssetData& section) {
			nlohmann::json node = nlohmann::json::object();
			node["sectionId"]   = section.sectionId;
			node["startFrame"]  = section.startFrame;
			node["endFrame"]    = section.endFrame;
			node["priority"]    = section.priority;

			node["floatCurve"] = ToJsonCurve(section.floatCurve);
			node["vec3XCurve"] = ToJsonCurve(section.vec3XCurve);
			node["vec3YCurve"] = ToJsonCurve(section.vec3YCurve);
			node["vec3ZCurve"] = ToJsonCurve(section.vec3ZCurve);
			node["transformPosX"] = ToJsonCurve(section.transformPosX);
			node["transformPosY"] = ToJsonCurve(section.transformPosY);
			node["transformPosZ"] = ToJsonCurve(section.transformPosZ);
			node["transformRotX"] = ToJsonCurve(section.transformRotX);
			node["transformRotY"] = ToJsonCurve(section.transformRotY);
			node["transformRotZ"] = ToJsonCurve(section.transformRotZ);
			node["transformRotW"] = ToJsonCurve(section.transformRotW);
			node["transformScaleX"] = ToJsonCurve(section.transformScaleX);
			node["transformScaleY"] = ToJsonCurve(section.transformScaleY);
			node["transformScaleZ"] = ToJsonCurve(section.transformScaleZ);

			node["boolKeys"] = nlohmann::json::array();
			for (const SequenceBoolKeyAssetData& key : section.boolKeys) {
				node["boolKeys"].emplace_back(
					nlohmann::json::object(
						{
							{"keyId", key.keyId},
							{"frame", key.frame},
							{"value", key.value}
						}
					)
				);
			}

			node["cameraCutKeys"] = nlohmann::json::array();
			for (const SequenceCameraCutKeyAssetData& key : section.cameraCutKeys) {
				node["cameraCutKeys"].emplace_back(
					nlohmann::json::object(
						{
							{"keyId", key.keyId},
							{"frame", key.frame},
							{"cameraEntityGuid", key.cameraEntityGuid}
						}
					)
				);
			}

			node["eventKeys"] = nlohmann::json::array();
			for (const SequenceEventKeyAssetData& key : section.eventKeys) {
				node["eventKeys"].emplace_back(
					nlohmann::json::object(
						{
							{"keyId", key.keyId},
							{"frame", key.frame},
							{"cueId", key.cueId},
							{"sourceEntityGuid", key.sourceEntityGuid},
							{"cueValue", key.cueValue},
							{"cueValue2", key.cueValue2},
							{"consoleCommand", key.consoleCommand}
						}
					)
				);
			}

			node["skeletal"] = nlohmann::json::object(
				{
					{"stateId", section.skeletal.stateId},
					{"layerIndex", section.skeletal.layerIndex},
					{"playOnEnter", section.skeletal.playOnEnter},
					{"stopOnExit", section.skeletal.stopOnExit},
					{"weightCurve", ToJsonCurve(section.skeletal.weightCurve)},
					{"playbackCurve", ToJsonCurve(section.skeletal.playbackCurve)},
					{"speedCurve", ToJsonCurve(section.skeletal.speedCurve)}
				}
			);

			return node;
		}

		[[nodiscard]] uint64_t ComputeSemanticHash(const std::string& text) {
			return static_cast<uint64_t>(std::hash<std::string>{}(text));
		}

		[[nodiscard]] std::string ReadTextFile(const std::string& path) {
			std::ifstream ifs(Path::FromUtf8(path), std::ios::binary);
			if (!ifs) {
				return {};
			}
			std::ostringstream oss;
			oss << ifs.rdbuf();
			return oss.str();
		}
	}

	bool SequenceFileIO::LoadFromFile(
		const std::string&      path,
		SequenceFileLoadResult& outResult
	) {
		const std::string text = ReadTextFile(path);
		if (text.empty()) {
			return false;
		}

		nlohmann::json root = nlohmann::json::object();
		try {
			root = nlohmann::json::parse(text);
		} catch (...) {
			return false;
		}

		bool migrated = false;
		int32_t sourceVersion = 1;
		if (!SequenceMigrator::MigrateToLatest(root, &migrated, &sourceVersion)) {
			return false;
		}

		SequenceAuthoringData authoring = {};
		authoring.version = root.value("version", 2);
		const std::filesystem::path filePath = Path::FromUtf8(path);
		authoring.name = root.value(
			"name",
			Path::ToUtf8String(filePath.stem().stem())
		);
		authoring.displayRate = std::max(1, root.value("displayRate", 60));
		authoring.tickResolution = std::max(1, root.value("tickResolution", 24000));
		authoring.lengthFrames = std::max<int64_t>(0, root.value("lengthFrames", 0ll));

		if (root.contains("bindings") && root["bindings"].is_array()) {
			authoring.bindings.reserve(root["bindings"].size());
			for (const nlohmann::json& bindingNode : root["bindings"]) {
				if (!bindingNode.is_object()) {
					continue;
				}
				authoring.bindings.emplace_back(
					SequenceBindingAssetData{
						.bindingId = bindingNode.value("bindingId", 0ull),
						.entityGuid = bindingNode.value("entityGuid", 0ull),
						.componentStableName = bindingNode.value(
							"componentStableName",
							std::string()
						),
						.propertyPath = bindingNode.value("propertyPath", std::string()),
					}
				);
			}
		}

		if (root.contains("tracks") && root["tracks"].is_array()) {
			authoring.tracks.reserve(root["tracks"].size());
			for (const nlohmann::json& trackNode : root["tracks"]) {
				if (!trackNode.is_object()) {
					continue;
				}

				SequenceTrackAssetData track = {};
				track.trackId = trackNode.value("trackId", 0ull);
				track.name = trackNode.value("name", std::string());
				if (trackNode.contains("trackType")) {
					track.trackType = ParseTrackType(trackNode["trackType"]);
				}
				if (trackNode.contains("blendMode")) {
					track.blendMode = ParseBlendMode(trackNode["blendMode"]);
				}
				track.priority  = trackNode.value("priority", 0);
				track.bindingId = trackNode.value("bindingId", 0ull);

				if (trackNode.contains("sections") && trackNode["sections"].is_array()) {
					track.sections.reserve(trackNode["sections"].size());
					for (const nlohmann::json& sectionNode : trackNode["sections"]) {
						track.sections.emplace_back(ParseSection(sectionNode));
					}
					std::ranges::sort(
						track.sections,
						[](const SequenceSectionAssetData& lhs, const SequenceSectionAssetData& rhs) {
							return lhs.startFrame < rhs.startFrame;
						}
					);
				}

				for (const SequenceSectionAssetData& section : track.sections) {
					authoring.lengthFrames = std::max(authoring.lengthFrames, section.endFrame);
				}

				authoring.tracks.emplace_back(std::move(track));
			}
		}

		if (root.contains("editor") && root["editor"].is_object()) {
			const nlohmann::json& editorNode = root["editor"];
			authoring.editor.scrubFireEvents = editorNode.value("scrubFireEvents", false);
			authoring.editor.autoKeyEnabled = editorNode.value("autoKeyEnabled", false);
		}

		outResult.authoring     = authoring;
		outResult.runtime       = BuildRuntimeData(authoring);
		outResult.migrated      = migrated;
		outResult.sourceVersion = sourceVersion;
		outResult.semanticHash  = ComputeSemanticHash(root.dump());
		return true;
	}

	bool SequenceFileIO::SaveToFile(
		const std::string&           path,
		const SequenceAuthoringData& authoring,
		bool*                        outWritten
	) {
		nlohmann::json root = nlohmann::json::object();
		root["version"]        = std::max(2, authoring.version);
		root["name"]           = authoring.name;
		root["displayRate"]    = authoring.displayRate;
		root["tickResolution"] = authoring.tickResolution;
		root["lengthFrames"]   = authoring.lengthFrames;

		root["bindings"] = nlohmann::json::array();
		for (const SequenceBindingAssetData& binding : authoring.bindings) {
			root["bindings"].emplace_back(
				nlohmann::json::object(
					{
						{"bindingId", binding.bindingId},
						{"entityGuid", binding.entityGuid},
						{"componentStableName", binding.componentStableName},
						{"propertyPath", binding.propertyPath}
					}
				)
			);
		}

		root["tracks"] = nlohmann::json::array();
		for (const SequenceTrackAssetData& track : authoring.tracks) {
			nlohmann::json trackNode = nlohmann::json::object();
			trackNode["trackId"]   = track.trackId;
			trackNode["name"]      = track.name;
			trackNode["trackType"] = ToJsonTrackType(track.trackType);
			trackNode["blendMode"] = ToJsonBlendMode(track.blendMode);
			trackNode["priority"]  = track.priority;
			trackNode["bindingId"] = track.bindingId;
			trackNode["sections"]  = nlohmann::json::array();
			for (const SequenceSectionAssetData& section : track.sections) {
				trackNode["sections"].emplace_back(ToJsonSection(section));
			}
			root["tracks"].emplace_back(std::move(trackNode));
		}

		root["editor"] = nlohmann::json::object(
			{
				{"scrubFireEvents", authoring.editor.scrubFireEvents},
				{"autoKeyEnabled", authoring.editor.autoKeyEnabled}
			}
		);

		const std::string nextText     = root.dump(4);
		const std::string currentText  = ReadTextFile(path);
		if (currentText == nextText) {
			if (outWritten) {
				*outWritten = false;
			}
			return true;
		}

		const std::filesystem::path filePath = Path::FromUtf8(path);
		if (filePath.has_parent_path()) {
			std::error_code ec = {};
			(void)std::filesystem::create_directories(filePath.parent_path(), ec);
		}

		std::ofstream ofs(
			Path::FromUtf8(path),
			std::ios::binary | std::ios::trunc
		);
		if (!ofs) {
			return false;
		}
		ofs.write(nextText.data(), static_cast<std::streamsize>(nextText.size()));
		ofs.flush();
		if (outWritten) {
			*outWritten = true;
		}
		return true;
	}

	SequenceAssetData SequenceFileIO::BuildRuntimeData(
		const SequenceAuthoringData& authoring
	) {
		SequenceAssetData runtime = {};
		runtime.version        = authoring.version;
		runtime.name           = authoring.name;
		runtime.displayRate    = authoring.displayRate;
		runtime.tickResolution = authoring.tickResolution;
		runtime.lengthFrames   = authoring.lengthFrames;
		runtime.bindings       = authoring.bindings;
		runtime.tracks         = authoring.tracks;
		return runtime;
	}
}
