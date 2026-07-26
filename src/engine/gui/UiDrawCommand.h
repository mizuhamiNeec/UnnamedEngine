#pragma once
#include <string>

#include "core/assets/AssetID.h"

#include "Rect.h"

namespace Unnamed::Gui {
	/// @brief Colorは、GUI描画命令へ渡す非premultiplied RGBA成分を保持します
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

	/// @brief UiDrawCommandRectは、GUI要素の位置と寸法を同じ座標系で表します
	struct UiDrawCommandRect {
		Rect  rect;
		Color fillColor;
		float cornerRadius    = 0.0f;
		float borderThickness = 0.0f;
		Color borderColor     = {.r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f};
	};

	/// @brief UiDrawCommandTextは、描画文字列、font、位置、色を1件のtext描画命令として保持します
	struct UiDrawCommandText {
		Vec2        position;
		std::string text;
		Color       color;
		float       fontSize = 16.0f;
	};

	/// @brief UiDrawCommandImageは、texture、描画矩形、UV、tintを1件のimage描画命令として保持します
	struct UiDrawCommandImage {
		Rect    rect;
		AssetID textureAssetId = kInvalidAssetID;
		Color color       = {.r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 1.0f};
		Vec2  uvMin       = Vec2(0.0f, 0.0f);
		Vec2  uvMax       = Vec2(1.0f, 1.0f);
		Vec2  anchor      = Vec2(0.0f, 0.0f);
		float rotationRad = 0.0f;
	};

	/// @brief UiDrawCommandは、GUIで順序付き実行する命令と引数を保持します
	struct UiDrawCommand {
		UI_DRAW_COMMAND_TYPE type{UI_DRAW_COMMAND_TYPE::RECT};
		UiDrawCommandRect    rect;
		UiDrawCommandText    text;
		UiDrawCommandImage   image;
	};
}
