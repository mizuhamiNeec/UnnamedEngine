#pragma once

#include "TweenTypes.h"

namespace Unnamed {
	/// @brief 正規化時間へイージング曲線を適用する純粋関数群を提供します
	class TweenEase {
	public:
		/// @brief 指定されたイージングタイプに基づいて、正規化時間に対する補間係数を計算します。
		/// @param easeType イージングタイプ。
		/// @param x 時間。0.0fから1.0fの範囲で指定します。
		static float Evaluate(EASE_TYPE easeType, float x);
	};
}
