#pragma once

#include <cfloat>
#include <cstdint>
#include <string>
#include <vector>

#include "core/math/Vec2.h"
#include "game/core/presentation/EventPresentationTypes.h"

namespace Unnamed {
	/// @brief EventPresentation 専用グラフノード ID です。
	using EventPresentationNodeId = uint64_t;
	/// @brief EventPresentation 専用グラフピン ID です。
	using EventPresentationPinId = uint64_t;
	/// @brief EventPresentation 専用グラフリンク ID です。
	using EventPresentationLinkId = uint64_t;

	/// @brief EventPresentation 専用グラフのノード種別です。
	enum class EventPresentationNodeKind : uint8_t {
		Trigger = 0,
		ValueSource,
		ValueTransform,
		Action,
		Comment,
		Reroute,
	};

	/// @brief グラフピンの入出力方向です。
	enum class EventPresentationPinDirection : uint8_t {
		Input = 0,
		Output,
	};

	/// @brief グラフピンの値型です。
	enum class EventPresentationValueType : uint8_t {
		Exec = 0,
		Float,
		Bool,
		Vec2,
		Vec3,
		EntityId,
	};

	/// @brief EventPresentation 専用グラフのピン定義です。
	struct EventPresentationPinData final {
		EventPresentationPinId        id        = 0;
		EventPresentationNodeId       nodeId    = 0;
		std::string                   name;
		EventPresentationPinDirection direction =
			EventPresentationPinDirection::Input;
		EventPresentationValueType valueType = EventPresentationValueType::Exec;
	};

	/// @brief EventPresentation 専用グラフのリンク定義です。
	struct EventPresentationLinkData final {
		EventPresentationLinkId id        = 0;
		EventPresentationPinId  fromPinId = 0;
		EventPresentationPinId  toPinId   = 0;
	};

	/// @brief EventPresentation 専用ノードの編集データです。
	struct EventPresentationNodeData final {
		EventPresentationNodeId id    = 0;
		EventPresentationNodeKind kind = EventPresentationNodeKind::Trigger;
		std::string              title;
		Vec2                     position = Vec2(0.0f, 0.0f);

		// Trigger / Action 共通の実行フローピン
		EventPresentationPinId execInPinId  = 0;
		EventPresentationPinId execOutPinId = 0;
		// Float 値入出力ピン
		EventPresentationPinId floatInPinId  = 0;
		EventPresentationPinId floatOutPinId = 0;

		// Trigger node properties
		std::string cueId;
		float       cooldownSec     = 0.0f;
		bool        conditionEnabled = false;
		float       conditionMin    = -FLT_MAX;
		float       conditionMax    = FLT_MAX;

		// ValueSource node properties
		EventPresentationValueSource valueSource =
			EventPresentationValueSource::CueValue;
		std::string payloadName;
		float       constant = 0.0f;

		// ValueTransform node properties
		bool  clampEnabled = false;
		float clampMin     = -FLT_MAX;
		float clampMax     = FLT_MAX;
		float multiply     = 1.0f;

		// Action node properties
		std::string actionType;
		std::string actionId;
		std::string debugText;

		// Comment node properties
		std::string commentText;
	};

	/// @brief EventPresentation 専用グラフ編集データ本体です。
	class EventPresentationEditorGraph final {
	public:
		/// @brief グラフを初期状態にリセットします。
		void Clear();

		/// @brief ノードを追加します。
		/// @param kind ノード種別
		/// @param title ノードタイトル
		/// @param position キャンバス上の配置座標
		/// @return 追加したノード ID
		[[nodiscard]] EventPresentationNodeId AddNode(
			EventPresentationNodeKind kind,
			const std::string&        title,
			const Vec2&               position
		);

		/// @brief ピンを追加します。
		/// @param nodeId 所有ノード ID
		/// @param name ピン表示名
		/// @param direction ピン方向
		/// @param valueType 値型
		/// @return 追加したピン ID (失敗時 0)
		[[nodiscard]] EventPresentationPinId AddPin(
			EventPresentationNodeId       nodeId,
			const std::string&            name,
			EventPresentationPinDirection direction,
			EventPresentationValueType    valueType
		);

		/// @brief リンクを追加します。
		/// @param fromPinId 出力側ピン ID
		/// @param toPinId 入力側ピン ID
		/// @return 追加したリンク ID (失敗時 0)
		[[nodiscard]] EventPresentationLinkId AddLink(
			EventPresentationPinId fromPinId,
			EventPresentationPinId toPinId
		);

		/// @brief リンクを削除します。
		/// @param linkId 削除対象リンク ID
		/// @return 削除できた場合 true
		[[nodiscard]] bool RemoveLink(EventPresentationLinkId linkId);

		/// @brief ノードと関連ピン/リンクを削除します。
		/// @param nodeId 削除対象ノード ID
		/// @return 削除できた場合 true
		[[nodiscard]] bool RemoveNode(EventPresentationNodeId nodeId);

		/// @brief ノードを検索します。
		[[nodiscard]] EventPresentationNodeData* FindNode(
			EventPresentationNodeId id
		);
		/// @brief ノードを検索します (const)。
		[[nodiscard]] const EventPresentationNodeData* FindNode(
			EventPresentationNodeId id
		) const;

		/// @brief ピンを検索します。
		[[nodiscard]] EventPresentationPinData* FindPin(EventPresentationPinId id);
		/// @brief ピンを検索します (const)。
		[[nodiscard]] const EventPresentationPinData* FindPin(
			EventPresentationPinId id
		) const;

		/// @brief ノードに属するピン一覧を収集します。
		/// @param nodeId 対象ノード ID
		/// @param direction 収集方向
		/// @return 該当ピン ID 配列
		[[nodiscard]] std::vector<EventPresentationPinId> CollectPinsByDirection(
			EventPresentationNodeId       nodeId,
			EventPresentationPinDirection direction
		) const;

		/// @brief 現在のノード/ピン/リンク配列から次IDカウンタを再計算します。
		void RecomputeNextIds();

		std::string name;
		Vec2        viewPan  = Vec2(0.0f, 0.0f);
		float       viewZoom = 1.0f;
		std::vector<EventPresentationNodeData> nodes;
		std::vector<EventPresentationPinData>  pins;
		std::vector<EventPresentationLinkData> links;

	private:
		EventPresentationNodeId mNextNodeId = 1;
		EventPresentationPinId  mNextPinId  = 1;
		EventPresentationLinkId mNextLinkId = 1;
	};
}
