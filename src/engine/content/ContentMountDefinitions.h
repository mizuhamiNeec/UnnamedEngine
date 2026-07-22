#pragma once

#include <string_view>

namespace Unnamed::ContentMountId {
	inline constexpr std::string_view kCore = "Core";
	inline constexpr std::string_view kGame = "Game";
}

namespace Unnamed::ContentMountPriority {
	inline constexpr int kCore = 100;
	inline constexpr int kGame = 200;
}
