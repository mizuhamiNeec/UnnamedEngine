#pragma once
#include "core/math/Vec2.h"

namespace Unnamed::UI {
	struct UnnamedUiInputState {
		Vec2 mousePosition = Vec2::zero;
		bool mouseDown     = false;
		bool mousePressed  = false;
		bool mouseReleased = false;
	};
}
