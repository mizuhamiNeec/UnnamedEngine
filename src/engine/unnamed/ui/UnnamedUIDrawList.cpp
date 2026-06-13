#include "UnnamedUIDrawList.h"

namespace Unnamed::UI {
	void UIDrawList::Clear() {
		mCommands.clear();
	}

	void UIDrawList::AddRect(const UIRect& rect, const UIColor& color) {
		UIDrawCommand command;
		command.type  = UIDrawCommandType::RECT;
		command.rect  = rect;
		command.color = color;
		mCommands.emplace_back(std::move(command));
	}

	void UIDrawList::AddText(
		const std::string& text,
		const Vec2&        position,
		const UIColor&     color,
		const float        fontSize,
		const uint32_t     oversampleH,
		const uint32_t     oversampleV
	) {
		UIDrawCommand command;
		command.type            = UIDrawCommandType::TEXT;
		command.text            = text;
		command.textPosition    = position;
		command.textFontSize    = fontSize;
		command.textOversampleH = oversampleH;
		command.textOversampleV = oversampleV;
		command.color           = color;
		mCommands.emplace_back(std::move(command));
	}

	void UIDrawList::AddBorder(
		const UIRect& rect, const float width, const UIColor& color
	) {
		// ボーダーが中心より太い場合は、矩形サイズに応じて幅を自動調整します。
		const float borderWidth = std::min(
			width,
			std::min(rect.size.x, rect.size.y) * 0.5f
		);

		// 上
		AddRect(
			UIRect{
				.position = rect.position,
				.size     = Vec2(rect.size.x, borderWidth)
			},
			color
		);

		// 下
		AddRect(
			UIRect{
				.position = Vec2(rect.position.x,
				                 rect.position.y + rect.size.y - borderWidth),
				.size = Vec2(rect.size.x, borderWidth)
			},
			color
		);

		// 左
		AddRect(
			UIRect{
				.position = rect.position,
				.size     = Vec2(borderWidth, rect.size.y)
			},
			color
		);

		// 右
		AddRect(
			UIRect{
				.position = Vec2(rect.position.x + rect.size.x - borderWidth,
				                 rect.position.y),
				.size = Vec2(borderWidth, rect.size.y)
			},
			color
		);
	}

	const std::vector<UIDrawCommand>& UIDrawList::GetCommands() const {
		return mCommands;
	}
}
