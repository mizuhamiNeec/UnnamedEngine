#pragma once

#include <cstdint>

#include "UnnamedUITypes.h"

namespace Unnamed::UI {
	/// @brief UI描画の最小テーマ定義です。
	struct UITheme {
		UIColor buttonNormalColor = {
			.r = 0.62f, .g = 0.62f, .b = 0.62f, .a = 1.0f
		};
		UIColor buttonHoveredColor = {
			.r = 0.33f, .g = 0.33f, .b = 0.33f, .a = 1.0f
		};
		UIColor buttonPressedColor = {
			.r = 0.65f, .g = 0.15f, .b = 0.15f, .a = 1.0f
		};
		UIColor panelColor = {
			.r = 0.69f, .g = 0.69f, .b = 0.69f, .a = 1.0f
		};
		UIColor textColor = {
			.r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 1.0f
		};
		float panelPadding = 2.0f;
		Vec2  buttonTextPadding = Vec2(12.0f, 12.0f);
		float fontSize          = 20.0f;
		uint32_t fontOversampleH = 1;
		uint32_t fontOversampleV = 1;
		
		float   defaultGap = 2.0f;
		float   defaultButtonHeight = 18.0f;
	};
}
