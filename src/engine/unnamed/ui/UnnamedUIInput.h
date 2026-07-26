#pragma once
#include "core/math/Vec2.h"

namespace Unnamed::UI {
	/// @brief UnnamedUiInputStateは、mouse位置、button遷移、wheel量を即時UIの1frame入力として保持します
	struct UnnamedUiInputState {
		Vec2 mousePosition = Vec2::zero;
		bool mouseDown     = false;
		bool mousePressed  = false;
		bool mouseReleased = false;
	};
}
