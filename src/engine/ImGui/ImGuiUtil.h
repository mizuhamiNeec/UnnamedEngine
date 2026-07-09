#pragma once

#ifdef _DEBUG
#include <imgui.h>
struct Vec3;
struct Vec4;

namespace ImGuiUtil {
#ifdef _DEBUG
	enum class HeaderMenuAction {
		None = 0,
		MoveUp,
		MoveDown,
		Remove,
	};

	ImVec4 ToImVec4(const Vec4& vec);

	void StyleColorsDark();
	void StyleColorsLight();

	void PushStyleColorForDrag(
		const ImVec4& bg, const ImVec4& bgHovered,
		const ImVec4& bgActive
	);
	void TextOutlined(
		ImDrawList* drawList, const ImVec2& pos, const char* text,
		ImVec4      textColor, ImVec4       outlineColor,
		float       outlineSize = 1.0f
	);

	/// @brief 折りたたみヘッダーとチェックボックスを組み合わせたウィジェットを表示します。
	/// @param icon アイコン（フォントアイコン）
	/// @param label ヘッダーのラベル
	/// @param id ヘッダーのID。GUIDを突っ込む
	/// @param checkbox チェックボックスの状態への参照
	/// @param action ヘッダーメニューで選択されたアクション
	/// @param canMoveUp 「上に移動」項目を有効化するかどうか
	/// @param canMoveDown 「下に移動」項目を有効化するかどうか
	/// @param canRemove 「削除」項目を有効化するかどうか
	/// @param flags ImGuiTreeNodeFlagsのフラグ
	/// @return ヘッダーが開かれた場合にtrueを返します
	/// @details 主にインスペクタで使用します。
	bool CollapsingHeaderWithCheckbox(
		uint32_t           icon,
		const char*        label,
		uint64_t           id,
		bool*              checkbox,
		HeaderMenuAction*  action,
		bool               canMoveUp   = true,
		bool               canMoveDown = true,
		bool               canRemove   = true,
		ImGuiTreeNodeFlags flags       = 0
	);
#endif
}
#endif
