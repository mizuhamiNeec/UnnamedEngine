#include "EventPresentationExecutor.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "game/core/components/AudioFxControllerComponent.h"
#include "game/core/components/CameraFxControllerComponent.h"

#include "engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel = "EventPresentationV2";

		[[nodiscard]] std::string TrimAscii(std::string_view text) {
			size_t begin = 0;
			while (
				begin < text.size() &&
				std::isspace(static_cast<unsigned char>(text[begin])) != 0
			) {
				++begin;
			}
			size_t end = text.size();
			while (
				end > begin &&
				std::isspace(static_cast<unsigned char>(text[end - 1])) != 0
			) {
				--end;
			}
			return std::string(text.substr(begin, end - begin));
		}

		[[nodiscard]] std::string ToLowerAscii(std::string text) {
			std::ranges::transform(
				text,
				text.begin(),
				[](const unsigned char c) {
					return static_cast<char>(std::tolower(c));
				}
			);
			return text;
		}

		[[nodiscard]] float ReadSourceValue(
			const EventPresentationValueSource source,
			const float                        constantValue,
			const std::string_view             payloadName,
			const GameplayCue&                 cue
		) {
			switch (source) {
				case EventPresentationValueSource::CueValue: return cue.value;
				case EventPresentationValueSource::CueValue2: return cue.value2;
				case EventPresentationValueSource::PayloadFloat: {
					if (payloadName.empty()) {
						Warning(
							kChannel,
							"Value source payload.xxx is missing payload name (cue='{}').",
							cue.id
						);
						return 0.0f;
					}
					float payloadValue = 0.0f;
					if (!cue.TryGetFloat(payloadName, payloadValue)) {
						Warning(
							kChannel,
							"Missing or non-float payload '{}' for cue='{}' (sourceGuid={}).",
							payloadName,
							cue.id,
							cue.sourceEntityGuid
						);
						return 0.0f;
					}
					return payloadValue;
				}
				case EventPresentationValueSource::Constant: return constantValue;
				default: return cue.value;
			}
		}
	}

	EventPresentationValueSource EventPresentationExecutor::ParseValueSource(
		const std::string_view sourceText,
		std::string*           outPayloadName
	) {
		if (outPayloadName) {
			outPayloadName->clear();
		}
		const std::string normalized = ToLowerAscii(TrimAscii(sourceText));
		if (normalized == "cue.value") {
			return EventPresentationValueSource::CueValue;
		}
		if (normalized == "cue.value2" || normalized == "cue.impact_speed") {
			return EventPresentationValueSource::CueValue2;
		}
		if (normalized.rfind("payload.", 0) == 0) {
			std::string payloadName = TrimAscii(
				std::string_view(normalized).substr(
					std::string_view("payload.").size()
				)
			);
			if (payloadName.empty()) {
				return EventPresentationValueSource::CueValue;
			}
			if (outPayloadName) {
				*outPayloadName = std::move(payloadName);
			}
			return EventPresentationValueSource::PayloadFloat;
		}
		if (normalized == "constant") {
			return EventPresentationValueSource::Constant;
		}
		return EventPresentationValueSource::CueValue;
	}

	EventPresentationActionType EventPresentationExecutor::ParseActionType(
		const std::string_view actionTypeText
	) {
		const std::string normalized = ToLowerAscii(TrimAscii(actionTypeText));
		if (normalized == "sound.play") {
			return EventPresentationActionType::SoundPlay;
		}
		if (normalized == "camera.shake") {
			return EventPresentationActionType::CameraShake;
		}
		if (normalized == "camera.fov") {
			return EventPresentationActionType::CameraFov;
		}
		if (normalized == "camera.rotation") {
			return EventPresentationActionType::CameraRotation;
		}
		if (normalized == "animation.play_state") {
			return EventPresentationActionType::AnimationPlayState;
		}
		if (normalized == "animation.set_speed") {
			return EventPresentationActionType::AnimationSetSpeed;
		}
		if (normalized == "debug.print") {
			return EventPresentationActionType::DebugPrint;
		}
		return EventPresentationActionType::Unknown;
	}

	bool EventPresentationExecutor::EvaluateCondition(
		const EventPresentationCondition& condition,
		const GameplayCue&                cue
	) {
		if (!condition.enabled) {
			return true;
		}
		const float value = ReadSourceValue(
			condition.source,
			0.0f,
			condition.payloadName,
			cue
		);
		return value >= condition.minValue && value <= condition.maxValue;
	}

	float EventPresentationExecutor::EvaluateValue(
		const EventPresentationValuePipeline& pipeline,
		const GameplayCue&                    cue
	) {
		float value = ReadSourceValue(
			pipeline.source,
			pipeline.constant,
			pipeline.payloadName,
			cue
		);
		if (pipeline.clampEnabled) {
			value = std::clamp(value, pipeline.clampMin, pipeline.clampMax);
		}
		return value * pipeline.multiply;
	}

	void EventPresentationExecutor::ExecuteActions(
		const EventPresentationTrigger& trigger,
		const ExecutionContext&         context
	) {
		const auto reportAction = [&](const size_t actionIndex,
		                              const ActionTraceStatus status) {
			if (context.actionTraceCallback) {
				context.actionTraceCallback(actionIndex, status);
			}
		};

		for (size_t actionIndex = 0; actionIndex < trigger.actions.size();
		     ++actionIndex) {
			const EventPresentationAction& action = trigger.actions[actionIndex];
			const float actionValue = EvaluateValue(action.value, context.cue);
			switch (action.actionType) {
				case EventPresentationActionType::SoundPlay: {
					if (!context.audioFx) {
						Warning(
							kChannel,
							"Action sound.play failed: missing AudioFxController (receiverGuid={} cue='{}').",
							context.receiverEntityGuid,
							context.cue.id
						);
						reportAction(actionIndex, ActionTraceStatus::Error);
						break;
					}
					if (action.id.empty()) {
						Warning(
							kChannel,
							"Action sound.play failed: preset id is empty (asset='{}').",
							context.assetName
						);
						reportAction(actionIndex, ActionTraceStatus::Error);
						break;
					}
					const float intensity = std::max(0.0f, actionValue);
					if (intensity <= 0.0f) {
						if (context.verboseLog) {
							DevMsg(
								kChannel,
								"Action sound.play skipped: value={:.3f} preset='{}'.",
								intensity,
								action.id
							);
						}
						reportAction(actionIndex, ActionTraceStatus::Skipped);
						break;
					}
					const bool triggered = context.audioFx->TriggerOneShot(
						action.id,
						intensity
					);
					if (!triggered) {
						Warning(
							kChannel,
							"Action sound.play failed: preset='{}' value={:.3f} targetGuid={}.",
							action.id,
							intensity,
							context.audioTargetEntityGuid
						);
						reportAction(actionIndex, ActionTraceStatus::Error);
					} else if (context.verboseLog) {
						DevMsg(
							kChannel,
							"Action sound.play preset='{}' value={:.3f} targetGuid={}.",
							action.id,
							intensity,
							context.audioTargetEntityGuid
						);
						reportAction(actionIndex, ActionTraceStatus::Executed);
					} else {
						reportAction(actionIndex, ActionTraceStatus::Executed);
					}
					break;
				}
				case EventPresentationActionType::CameraShake: {
					if (!context.cameraFx) {
						Warning(
							kChannel,
							"Action camera.shake failed: missing CameraFxController (receiverGuid={} cue='{}').",
							context.receiverEntityGuid,
							context.cue.id
						);
						reportAction(actionIndex, ActionTraceStatus::Error);
						break;
					}
					if (action.id.empty()) {
						Warning(
							kChannel,
							"Action camera.shake failed: preset id is empty (asset='{}').",
							context.assetName
						);
						reportAction(actionIndex, ActionTraceStatus::Error);
						break;
					}
					const float intensity = std::max(0.0f, actionValue);
					if (intensity <= 0.0f) {
						if (context.verboseLog) {
							DevMsg(
								kChannel,
								"Action camera.shake skipped: value={:.3f} preset='{}'.",
								intensity,
								action.id
							);
						}
						reportAction(actionIndex, ActionTraceStatus::Skipped);
						break;
					}
					context.cameraFx->TriggerShake(action.id, intensity);
					if (context.verboseLog) {
						DevMsg(
							kChannel,
							"Action camera.shake preset='{}' value={:.3f} targetGuid={}.",
							action.id,
							intensity,
							context.cameraTargetEntityGuid
						);
					}
					reportAction(actionIndex, ActionTraceStatus::Executed);
					break;
				}
				case EventPresentationActionType::CameraFov: {
					if (!context.cameraFx) {
						Warning(
							kChannel,
							"Action camera.fov failed: missing CameraFxController (receiverGuid={} cue='{}').",
							context.receiverEntityGuid,
							context.cue.id
						);
						reportAction(actionIndex, ActionTraceStatus::Error);
						break;
					}
					if (action.id.empty()) {
						Warning(
							kChannel,
							"Action camera.fov failed: preset id is empty (asset='{}').",
							context.assetName
						);
						reportAction(actionIndex, ActionTraceStatus::Error);
						break;
					}
					const float intensity = std::max(0.0f, actionValue);
					if (intensity <= 0.0f) {
						if (context.verboseLog) {
							DevMsg(
								kChannel,
								"Action camera.fov skipped: value={:.3f} preset='{}'.",
								intensity,
								action.id
							);
						}
						reportAction(actionIndex, ActionTraceStatus::Skipped);
						break;
					}
					context.cameraFx->TriggerFov(action.id, intensity);
					if (context.verboseLog) {
						DevMsg(
							kChannel,
							"Action camera.fov preset='{}' value={:.3f} targetGuid={}.",
							action.id,
							intensity,
							context.cameraTargetEntityGuid
						);
					}
					reportAction(actionIndex, ActionTraceStatus::Executed);
					break;
				}
				case EventPresentationActionType::CameraRotation: {
					if (!context.cameraFx) {
						Warning(
							kChannel,
							"Action camera.rotation failed: missing CameraFxController (receiverGuid={} cue='{}').",
							context.receiverEntityGuid,
							context.cue.id
						);
						reportAction(actionIndex, ActionTraceStatus::Error);
						break;
					}
					if (action.id.empty()) {
						Warning(
							kChannel,
							"Action camera.rotation failed: preset id is empty (asset='{}').",
							context.assetName
						);
						reportAction(actionIndex, ActionTraceStatus::Error);
						break;
					}
					const float intensity = std::max(0.0f, actionValue);
					if (intensity <= 0.0f) {
						if (context.verboseLog) {
							DevMsg(
								kChannel,
								"Action camera.rotation skipped: value={:.3f} preset='{}'.",
								intensity,
								action.id
							);
						}
						reportAction(actionIndex, ActionTraceStatus::Skipped);
						break;
					}
					context.cameraFx->TriggerRotation(action.id, intensity);
					if (context.verboseLog) {
						DevMsg(
							kChannel,
							"Action camera.rotation preset='{}' value={:.3f} targetGuid={}.",
							action.id,
							intensity,
							context.cameraTargetEntityGuid
						);
					}
					reportAction(actionIndex, ActionTraceStatus::Executed);
					break;
				}
				case EventPresentationActionType::AnimationPlayState: {
					if (!context.animation) {
						Warning(
							kChannel,
							"Action animation.play_state failed: missing SkeletalAnimation (receiverGuid={} cue='{}').",
							context.receiverEntityGuid,
							context.cue.id
						);
						reportAction(actionIndex, ActionTraceStatus::Error);
						break;
					}
					if (action.id.empty()) {
						Warning(
							kChannel,
							"Action animation.play_state failed: state id is empty (asset='{}').",
							context.assetName
						);
						reportAction(actionIndex, ActionTraceStatus::Error);
						break;
					}
					const bool played = context.animation->PlayState(action.id);
					if (!played) {
						Warning(
							kChannel,
							"Action animation.play_state failed: state='{}' targetGuid={}.",
							action.id,
							context.animationTargetEntityGuid
						);
						reportAction(actionIndex, ActionTraceStatus::Error);
					} else if (context.verboseLog) {
						DevMsg(
							kChannel,
							"Action animation.play_state state='{}' targetGuid={}.",
							action.id,
							context.animationTargetEntityGuid
						);
						reportAction(actionIndex, ActionTraceStatus::Executed);
					} else {
						reportAction(actionIndex, ActionTraceStatus::Executed);
					}
					break;
				}
				case EventPresentationActionType::AnimationSetSpeed: {
					if (!context.animation) {
						Warning(
							kChannel,
							"Action animation.set_speed failed: missing SkeletalAnimation (receiverGuid={} cue='{}').",
							context.receiverEntityGuid,
							context.cue.id
						);
						reportAction(actionIndex, ActionTraceStatus::Error);
						break;
					}
					if (!std::isfinite(actionValue)) {
						Warning(
							kChannel,
							"Action animation.set_speed skipped: non-finite value={:.3f} (cue='{}' asset='{}').",
							actionValue,
							context.cue.id,
							context.assetName
						);
						reportAction(actionIndex, ActionTraceStatus::Warning);
						break;
					}
					const float safeSpeed = std::max(0.0f, actionValue);
					context.animation->SetSpeed(safeSpeed);
					if (context.verboseLog) {
						DevMsg(
							kChannel,
							"Action animation.set_speed value={:.3f} targetGuid={}.",
							safeSpeed,
							context.animationTargetEntityGuid
						);
					}
					reportAction(actionIndex, ActionTraceStatus::Executed);
					break;
				}
				case EventPresentationActionType::DebugPrint: {
					const std::string_view text = !action.debugText.empty() ?
						                              std::string_view(action.debugText) :
						                              std::string_view(action.id);
					DevMsg(
						kChannel,
						"[debug.print] cue='{}' value={:.3f} value2={:.3f} expr={:.3f} message='{}'.",
						context.cue.id,
						context.cue.value,
						context.cue.value2,
						actionValue,
						text
					);
					reportAction(actionIndex, ActionTraceStatus::Executed);
					break;
				}
				case EventPresentationActionType::Unknown:
				default:
					Warning(
						kChannel,
						"Unknown action type '{}' in trigger cue='{}' (asset='{}').",
						action.typeName,
						trigger.cueId,
						context.assetName
					);
					reportAction(actionIndex, ActionTraceStatus::Warning);
					break;
			}
		}
	}
}
