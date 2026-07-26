#pragma once

#include "TweenTypes.h"

namespace Unnamed {
	/// @brief 正規化時間へイージング曲線を適用する純粋関数群を提供します
	class TweenEase {
	public:
		static float Evaluate(EASE_TYPE easeType, float x);
	};
}
