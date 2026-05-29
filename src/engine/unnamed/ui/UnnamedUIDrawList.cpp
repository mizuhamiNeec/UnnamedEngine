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
		command.type         = UIDrawCommandType::TEXT;
		command.text         = text;
		command.textPosition = position;
		command.textFontSize = fontSize;
		command.textOversampleH = oversampleH;
		command.textOversampleV = oversampleV;
		command.color        = color;
		mCommands.emplace_back(std::move(command));
	}

	const std::vector<UIDrawCommand>& UIDrawList::GetCommands() const {
		return mCommands;
	}
}
