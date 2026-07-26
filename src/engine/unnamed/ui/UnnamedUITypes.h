#pragma once
#include "core/math/Vec2.h"

namespace Unnamed::UI {
	/// @brief 即時UIの位置と寸法をピクセル単位で表します
	struct UIRect {
		Vec2 position = Vec2::zero;
		Vec2 size     = Vec2::one;

		/// @brief 矩形内に点が含まれるか返します。
		/// @param point クライアント座標系の点（左上が原点、右下が正方向）
		[[nodiscard]] bool Contains(const Vec2 point) const {
			return point.x >= position.x &&
			       point.y >= position.y &&
			       point.x <= position.x + size.x &&
			       point.y <= position.y + size.y;
		}
	};

	/// @brief UIの色構造体
	/// RGBAで0.0f～1.0fの範囲で表現されます。
	struct UIColor {
		float r, g, b, a = 1.0f;
	};

	UIColor Lerp(const UIColor& a, const UIColor& b, float t);

	/// @brief UIテキストの水平配置です。
	enum class UI_TEXT_ALIGN : uint8_t {
		LEFT,
		CENTER,
		RIGHT,
	};
}
