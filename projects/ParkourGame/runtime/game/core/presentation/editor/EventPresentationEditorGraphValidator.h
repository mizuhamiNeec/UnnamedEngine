#pragma once

#include <string>
#include <vector>

#include "game/core/presentation/editor/EventPresentationEditorGraph.h"

namespace Unnamed {
	/// @brief EventPresentation グラフ検証結果の深刻度です。
	enum class EventPresentationValidationSeverity : uint8_t {
		Info = 0,
		Warning,
		Error,
	};

	/// @brief EventPresentation グラフ検証メッセージです。
	struct EventPresentationValidationIssue final {
		EventPresentationValidationSeverity severity =
			EventPresentationValidationSeverity::Info;
		std::string         message;
		EventPresentationNodeId nodeId = 0;
		EventPresentationLinkId linkId = 0;
	};

	/// @brief EventPresentation 専用グラフの静的バリデーションを行うクラスです。
	class EventPresentationEditorGraphValidator final {
	public:
		/// @brief グラフ全体を検証します。
		/// @param graph 検証対象グラフ
		/// @param outIssues 検証結果
		/// @return Error が 0 件なら true
		[[nodiscard]] static bool Validate(
			const EventPresentationEditorGraph&         graph,
			std::vector<EventPresentationValidationIssue>& outIssues
		);
	};
}

