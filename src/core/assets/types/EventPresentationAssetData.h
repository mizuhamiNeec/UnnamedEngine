#pragma once

#include <cfloat>
#include <string>
#include <vector>

namespace Unnamed {
	/// @brief Event Presentation v2 の値入力を定義します。
	struct EventPresentationValueInputAssetData {
		std::string source       = "cue.value";
		float       constant     = 0.0f;
		bool        clampEnabled = false;
		float       clampMin     = -FLT_MAX;
		float       clampMax     = FLT_MAX;
		float       multiply     = 1.0f;
	};

	/// @brief Event Presentation v2 の条件式を定義します。
	struct EventPresentationConditionAssetData {
		bool        enabled  = false;
		std::string source   = "cue.value";
		float       minValue = -FLT_MAX;
		float       maxValue = FLT_MAX;
	};

	/// @brief Event Presentation v2 の Action 設定を定義します。
	struct EventPresentationActionAssetData {
		std::string                          type;
		std::string                          id;
		std::string                          debugText;
		EventPresentationValueInputAssetData valueInput;
	};

	/// @brief Event Presentation v2 の Cue 反応定義を表します。
	struct EventPresentationTriggerAssetData {
		std::string                                   cueId;
		float                                         cooldownSec = 0.0f;
		EventPresentationConditionAssetData           condition;
		std::vector<EventPresentationActionAssetData> actions;
	};

	/// @brief Event Presentation v2 アセットのルートデータです。
	struct EventPresentationAssetData {
		std::string                                    name;
		std::vector<EventPresentationTriggerAssetData> triggers;
	};
}
