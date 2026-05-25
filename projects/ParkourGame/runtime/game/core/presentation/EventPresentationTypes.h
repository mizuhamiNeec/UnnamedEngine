#pragma once

#include <cfloat>
#include <cstdint>
#include <string>
#include <vector>

namespace Unnamed {
	/// @brief Event Presentation v2 で値を取得する入力元です。
	enum class EventPresentationValueSource : uint8_t {
		CueValue = 0,
		CueValue2,
		PayloadFloat,
		Constant,
	};

	/// @brief Event Presentation v2 で実行可能な Action 種別です。
	enum class EventPresentationActionType : uint8_t {
		Unknown = 0,
		SoundPlay,
		CameraShake,
		CameraFov,
		CameraRotation,
		AnimationPlayState,
		AnimationSetSpeed,
		DebugPrint,
	};

	/// @brief Event Presentation v2 の値加工パイプラインです。
	struct EventPresentationValuePipeline {
		EventPresentationValueSource source       =
			EventPresentationValueSource::CueValue;
		std::string payloadName;
		float constant     = 0.0f;
		bool  clampEnabled = false;
		float clampMin     = -FLT_MAX;
		float clampMax     = FLT_MAX;
		float multiply     = 1.0f;
	};

	/// @brief Event Presentation v2 のトリガー条件です。
	struct EventPresentationCondition {
		bool                        enabled  = false;
		EventPresentationValueSource source   =
			EventPresentationValueSource::CueValue;
		std::string payloadName;
		float minValue = -FLT_MAX;
		float maxValue = FLT_MAX;
	};

	/// @brief Event Presentation v2 の Action 実行設定です。
	struct EventPresentationAction {
		EventPresentationActionType actionType =
			EventPresentationActionType::Unknown;
		std::string                   typeName;
		std::string                   id;
		std::string                   debugText;
		EventPresentationValuePipeline value;
	};

	/// @brief Event Presentation v2 の Cue 反応定義です。
	struct EventPresentationTrigger {
		std::string                      cueId;
		float                            cooldownSec    = 0.0f;
		float                            lastTriggerAt  = -FLT_MAX;
		EventPresentationCondition       condition;
		std::vector<EventPresentationAction> actions;
	};
}
