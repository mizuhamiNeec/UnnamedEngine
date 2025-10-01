#pragma once

#ifdef _DEBUG
#include <imgui.h>
struct Vec4;

namespace ImGuiUtil {
#ifdef _DEBUG
	ImVec4 ToImVec4(const Vec4& vec);
#endif
}
#endif
