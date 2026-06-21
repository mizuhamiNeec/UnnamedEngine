#pragma once
#include <string>

#include "Rect.h"

namespace Unnamed::Gui {
	struct Color {
		float r = 1.0f;
		float g = 1.0f;
		float b = 1.0f;
		float a = 1.0f;
	};

	enum class UI_DRAW_COMMAND_TYPE {
		RECT,
		TEXT,
		IMAGE,
	};

	struct UiDrawCommandRect {
		Rect  rect;
		Color fillColor;
		float cornerRadius    = 0.0f;
		float borderThickness = 0.0f;
		Color borderColor     = {.r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f};
	};

	struct UiDrawCommandText {
		Vec2        position;
		std::string text;
		Color       color;
		float       fontSize = 16.0f;
	};

	struct UiDrawCommandImage {
		Rect  rect;
		Path  texturePath;
		Color color       = {.r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 1.0f};
		Vec2  uvMin       = Vec2(0.0f, 0.0f);
		Vec2  uvMax       = Vec2(1.0f, 1.0f);
		Vec2  anchor      = Vec2(0.0f, 0.0f);
		float rotationRad = 0.0f;
	};

	struct UiDrawCommand {
		UI_DRAW_COMMAND_TYPE type{UI_DRAW_COMMAND_TYPE::RECT};
		UiDrawCommandRect    rect;
		UiDrawCommandText    text;
		UiDrawCommandImage   image;
	};
}
