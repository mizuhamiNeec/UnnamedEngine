#pragma once

#ifdef _DEBUG
#include <imgui.h>
struct Vec3;
struct Vec4;

namespace ImGuiUtil {
#ifdef _DEBUG
	void StyleColorsDark();
	void StyleColorsLight();

	/// @brief アウトライン付きのテキストを描画します。
	/// @param drawList 描画リストへのポインタ
	/// @param pos テキストの位置
	/// @param text 描画するテキスト
	/// @param textColor テキストの色
	/// @param outlineColor アウトラインの色
	/// @param outlineSize アウトラインのサイズ
	void TextOutlined(
		ImDrawList* drawList, const ImVec2& pos, const char* text,
		ImVec4      textColor, ImVec4       outlineColor,
		float       outlineSize = 1.0f
	);
#endif
}
#endif
