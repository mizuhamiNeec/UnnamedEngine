#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "UnnamedUITypes.h"

namespace Unnamed::UI {
	enum class UIDrawCommandType {
		RECT,
		TEXT,
	};

	struct UIDrawCommand {
		UIDrawCommandType type = UIDrawCommandType::RECT;

		UIRect  rect;
		UIColor color;

		std::string text;
		Vec2        textPosition = Vec2::zero;
		float       textFontSize = 18.0f;
		uint32_t    textOversampleH = 1;
		uint32_t    textOversampleV = 1;
	};

	class UIDrawList {
	public:
		void Clear();

		void AddRect(const UIRect& rect, const UIColor& color);
		void AddText(
			const std::string& text,
			const Vec2&        position,
			const UIColor&     color,
			float              fontSize,
			uint32_t           oversampleH,
			uint32_t           oversampleV
		);

		[[nodiscard]] const std::vector<UIDrawCommand>& GetCommands() const;

	private:
		std::vector<UIDrawCommand> mCommands;
	};
}
