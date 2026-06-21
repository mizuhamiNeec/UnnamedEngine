#include "EventPresentationLoader.h"
#include "core/filesystem/Path.h"

#include <algorithm>
#include <filesystem>

#include "core/assets/types/EventPresentationAssetData.h"
#include "core/io/json/JsonReader.h"

#include "core/string/StrUtil.h"

namespace Unnamed {
	namespace {
		bool IsEventPresentationPath(const Path& path) {
			return StrUtil::ToLowerCase(path.ToGenericUtf8()).ends_with(
				".event_presentation.json"
			);
		}
	}

	bool EventPresentationLoader::CanLoad(
		const Path& path, ASSET_TYPE* outType
	) const {
		const bool ok = IsEventPresentationPath(path);
		if (outType) {
			*outType = ok ? ASSET_TYPE::EVENT_PRESENTATION : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult EventPresentationLoader::Load(const Path& path) {
		LoadResult       result = {};
		const JsonReader root(path);
		if (!root.Valid()) {
			return result;
		}

		const Path full = path.LexicallyNormal();
		EventPresentationAssetData  data = {};
		data.name = root.Read<std::string>("name").value_or(
			Path::ToUtf8String(full.Stem().Stem())
		);

		const JsonReader triggersNode = root["triggers"];
		if (triggersNode.Valid() && triggersNode.IsArray()) {
			for (size_t i = 0; i < triggersNode.Size(); ++i) {
				const JsonReader triggerNode = triggersNode[i];
				if (!triggerNode.Valid() || !triggerNode.IsObject()) {
					continue;
				}

				EventPresentationTriggerAssetData trigger = {};
				if (triggerNode.Has("cueId")) {
					trigger.cueId = triggerNode["cueId"].GetString("");
				}
				if (trigger.cueId.empty()) {
					continue;
				}
				if (triggerNode.Has("cooldownSec")) {
					trigger.cooldownSec = std::max(
						0.0f, triggerNode["cooldownSec"].GetFloat(0.0f)
					);
				}

				const JsonReader conditionNode = triggerNode["condition"];
				if (conditionNode.Valid() && conditionNode.IsObject()) {
					if (conditionNode.Has("enabled")) {
						trigger.condition.enabled =
							conditionNode["enabled"].GetBool(true);
					} else {
						trigger.condition.enabled = true;
					}
					if (conditionNode.Has("source")) {
						trigger.condition.source = conditionNode["source"].
							GetString(trigger.condition.source.c_str());
					}
					if (conditionNode.Has("min")) {
						trigger.condition.minValue = conditionNode["min"].GetFloat(
							trigger.condition.minValue
						);
					}
					if (conditionNode.Has("max")) {
						trigger.condition.maxValue = conditionNode["max"].GetFloat(
							trigger.condition.maxValue
						);
					}
					if (trigger.condition.maxValue < trigger.condition.minValue) {
						std::swap(
							trigger.condition.minValue,
							trigger.condition.maxValue
						);
					}
				}

				const JsonReader actionsNode = triggerNode["actions"];
				if (actionsNode.Valid() && actionsNode.IsArray()) {
					for (size_t actionIndex = 0; actionIndex < actionsNode.Size();
					     ++actionIndex) {
						const JsonReader actionNode = actionsNode[actionIndex];
						if (!actionNode.Valid() || !actionNode.IsObject()) {
							continue;
						}

						EventPresentationActionAssetData action = {};
						if (actionNode.Has("type")) {
							action.type = actionNode["type"].GetString("");
						}
						if (action.type.empty()) {
							continue;
						}
						if (actionNode.Has("id")) {
							action.id = actionNode["id"].GetString("");
						}
						if (actionNode.Has("debugText")) {
							action.debugText = actionNode["debugText"].GetString("");
						}

						const JsonReader valueNode = actionNode["value"];
						if (valueNode.Valid() && valueNode.IsObject()) {
							if (valueNode.Has("source")) {
								action.valueInput.source = valueNode["source"].
									GetString(action.valueInput.source.c_str());
							}
							if (valueNode.Has("constant")) {
								action.valueInput.constant = valueNode["constant"].
									GetFloat(action.valueInput.constant);
							}
							if (valueNode.Has("clampEnabled")) {
								action.valueInput.clampEnabled =
									valueNode["clampEnabled"].GetBool(
										action.valueInput.clampEnabled
									);
							}
							if (valueNode.Has("clampMin")) {
								action.valueInput.clampMin = valueNode["clampMin"].
									GetFloat(action.valueInput.clampMin);
							}
							if (valueNode.Has("clampMax")) {
								action.valueInput.clampMax = valueNode["clampMax"].
									GetFloat(action.valueInput.clampMax);
							}
							if (action.valueInput.clampMax <
							    action.valueInput.clampMin) {
								std::swap(
									action.valueInput.clampMin,
									action.valueInput.clampMax
								);
							}
							if (valueNode.Has("multiply")) {
								action.valueInput.multiply = valueNode["multiply"].
									GetFloat(action.valueInput.multiply);
							}
						}

						trigger.actions.emplace_back(std::move(action));
					}
				}

				if (!trigger.actions.empty()) {
					data.triggers.emplace_back(std::move(trigger));
				}
			}
		}

		result.payload     = std::move(data);
		result.resolveName = Path::ToUtf8String(full.Stem().Stem());

		std::error_code ec;
		if (std::filesystem::exists(path.Native(), ec)) {
			result.stamp.sizeInBytes = std::filesystem::file_size(
				path.Native(), ec
			);
		}

		return result;
	}
}
