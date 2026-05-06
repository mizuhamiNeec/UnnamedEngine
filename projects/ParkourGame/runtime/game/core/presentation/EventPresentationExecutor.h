#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string_view>

#include "engine/world/GameplayCueBus.h"
#include "game/core/presentation/EventPresentationTypes.h"

namespace Unnamed {
	class AudioFxControllerComponent;
	class CameraFxControllerComponent;
	class SkeletalAnimationComponent;

	/// @brief Event Presentation v2 の Action 実行器です。
	class EventPresentationExecutor final {
	public:
		/// @brief Action 1件の実行結果分類です（デバッグ可視化向け）。
		enum class ActionTraceStatus : uint8_t {
			Executed = 0,
			Skipped,
			Warning,
			Error
		};

		/// @brief 1回の Cue 処理に必要な実行コンテキストです。
		struct ExecutionContext {
			const GameplayCue&             cue;
			std::string_view               assetName;
			uint64_t                       receiverEntityGuid = 0;
			bool                           verboseLog = false;
			AudioFxControllerComponent*    audioFx    = nullptr;
			CameraFxControllerComponent*   cameraFx   = nullptr;
			SkeletalAnimationComponent*    animation  = nullptr;
			uint64_t                       audioTargetEntityGuid = 0;
			uint64_t                       cameraTargetEntityGuid = 0;
			uint64_t                       animationTargetEntityGuid = 0;
			std::function<void(size_t, ActionTraceStatus)> actionTraceCallback;
		};

		/// @brief 値入力元の文字列表現を列挙値へ変換します。
		/// @param sourceText JSON 側の source 文字列
		/// @param outPayloadName `payload.xxx` の `xxx` を受け取る出力先
		/// @return 変換結果
		[[nodiscard]] static EventPresentationValueSource ParseValueSource(
			std::string_view sourceText,
			std::string*     outPayloadName = nullptr
		);

		/// @brief Action 種別の文字列表現を列挙値へ変換します。
		/// @param actionTypeText JSON 側の type 文字列
		/// @return 変換結果
		[[nodiscard]] static EventPresentationActionType ParseActionType(
			std::string_view actionTypeText
		);

		/// @brief 条件判定を実行します。
		/// @param condition 判定条件
		/// @param cue 評価対象 Cue
		/// @return 条件を満たす場合 true
		[[nodiscard]] static bool EvaluateCondition(
			const EventPresentationCondition& condition,
			const GameplayCue&                cue
		);

		/// @brief 値加工パイプラインを評価します。
		/// @param pipeline 値加工設定
		/// @param cue 評価対象 Cue
		/// @return 加工後の値
		[[nodiscard]] static float EvaluateValue(
			const EventPresentationValuePipeline& pipeline,
			const GameplayCue&                    cue
		);

		/// @brief Trigger 内の Action 群を順次実行します。
		/// @param trigger 実行対象 Trigger
		/// @param context 実行コンテキスト
		static void ExecuteActions(
			const EventPresentationTrigger& trigger,
			const ExecutionContext&         context
		);
	};
}
