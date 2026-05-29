#pragma once
#include "core/math/Vec2.h"

namespace Unnamed::UI {
	/// @brief UIの矩形構造体
	struct UIRect {
		Vec2 position = Vec2::zero;
		Vec2 size     = Vec2::one;

		[[nodiscard]] bool Contains(const Vec2 point) const {
			return point.x >= position.x &&
			       point.y >= position.y &&
			       point.x <= position.x + size.x &&
			       point.y <= position.y + size.y;
		}
	};

	struct UIColor {
		float r, g, b, a = 1.0f;
	};
}
