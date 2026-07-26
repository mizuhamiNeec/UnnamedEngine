#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "UnnamedUITypes.h"

namespace Unnamed::UI {
	enum class UIDrawCommandType {
		RECT, // 矩形
		TEXT, // テキスト
	};

	/// @brief UIDrawCommandは、即時UIで順序付き実行する命令と引数を保持します
	struct UIDrawCommand {
		UIDrawCommandType type = UIDrawCommandType::RECT;

		UIRect  rect;
		UIColor color;

		Vec2  textPosition = Vec2::zero;
		float textFontSize = 18.0f;

		std::string text;

		uint32_t textOversampleH = 1;
		uint32_t textOversampleV = 1;
	};

	/// @brief UIDrawListは、即時UIが生成したスプライト描画命令を提出順に蓄積します
	class UIDrawList {
	public:
		void Clear();

		/// @brief 矩形を描画するコマンドを追加します。
		/// @param rect 描画する矩形
		/// @param color 矩形の色
		void AddRect(const UIRect& rect, const UIColor& color);

		/// @brief テキストを描画するコマンドを追加します。
		/// @param text 描画するテキスト
		/// @param position テキストの描画位置（左上基準）
		/// @param color テキストの色
		/// @param fontSize フォントサイズ（ピクセル単位）
		/// @param oversampleH 水平方向のオーバーサンプリング倍率
		/// @param oversampleV 垂直方向のオーバーサンプリング倍率
		void AddText(
			const std::string& text,
			const Vec2&        position,
			const UIColor&     color,
			float              fontSize,
			uint32_t           oversampleH,
			uint32_t           oversampleV
		);

		/// @brief 矩形の枠線を描画するコマンドを追加します。
		/// @param rect 枠線を描画する矩形
		/// @param width 枠線の幅
		/// @param color 枠線の色
		void AddBorder(const UIRect& rect, float width, const UIColor& color);

		[[nodiscard]] const std::vector<UIDrawCommand>& GetCommands() const;

	private:
		std::vector<UIDrawCommand> mCommands;
	};
}
