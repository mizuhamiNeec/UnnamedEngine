#pragma once

#include <cstdint>
#include <limits>
#include <string_view>

#include "core/hash/StableHashBuilder.h"

#include "core/math/Vec2.h"

namespace Unnamed::UI::Retained {
	/// @brief UIノードの識別子
	struct UiNodeId {
		constexpr static uint64_t kInvalid =
			std::numeric_limits<uint64_t>::max();

		uint64_t value = kInvalid;

		[[nodiscard]] bool IsValid() const {
			return value != kInvalid;
		}

		// 比較演算子
		auto operator<=>(const UiNodeId&) const = default;
	};

	/// @brief UIノードのハンドル
	struct UiNodeHandle {
		static constexpr uint32_t kInvalidIndex = std::numeric_limits<
			uint32_t>::max();

		uint32_t index      = kInvalidIndex;
		uint32_t generation = 0;

		[[nodiscard]] bool IsValid() const {
			return index != kInvalidIndex && generation != 0;
		}

		auto operator<=>(const UiNodeHandle&) const = default;
	};

	struct UiRect {
		Vec2 min = Vec2::zero;
		Vec2 max = Vec2::zero;

		/// @brief 矩形のサイズを取得します
		/// @return 矩形のサイズ
		[[nodiscard]] Vec2 Size() const {
			return max - min;
		}

		/// @brief 矩形の中心点を取得します
		/// @return 矩形の中心点
		[[nodiscard]] Vec2 Center() const {
			return (min + max) * 0.5f;
		}

		/// @brief 矩形が空かどうかを判定します
		/// @return 矩形が空であればtrue、そうでなければfalse
		[[nodiscard]] bool IsEmpty() const {
			const Vec2 size = Size();
			return size.x <= 0.0f || size.y <= 0.0f;
		}
	};

	enum class UI_NODE_TYPE : uint8_t {
		BUTTON,
		IMAGE,
	};

	enum class UI_INTERACTION_STATE : uint8_t {
		NORMAL,
		HOVERED,
		PRESSED,
		DISABLED,
	};

	struct UiInteractionResult {
		bool hovered = false;
		bool held    = false;
		bool clicked = false;
	};

	enum class UiEasing : uint8_t {
		LINEAR,
	};

	enum class UI_EVENT_TYPE : uint8_t {
		CLICK
	};

	struct UiEvent {
		UiNodeId      target = {};
		UI_EVENT_TYPE type   = UI_EVENT_TYPE::CLICK;
	};

	constexpr UiNodeId MakeUiNodeId(const std::string_view value) {
		StableHashBuilder builder;

		builder.AddString(value);

		return UiNodeId{builder.Value()};
	}
}
