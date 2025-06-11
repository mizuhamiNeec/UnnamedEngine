#pragma once
#include <string>

#include "Lib/Math/Vector/Vec3.h"

#ifdef _DEBUG 
#include <imgui.h>

namespace ImGuiWidgets {
	void PushStyleColorForDrag(const ImVec4& bg, const ImVec4& bgHovered, const ImVec4& bgActive);
	bool DragVec3(const std::string& name, Vec3& v, const float& vSpeed, const char* format);
	bool EditCubicBezier(const std::string& label, float& p0, float& p1, float& p2, float& p3);
	bool IconButton(const char* icon, const char* label, const ImVec2& size = ImVec2(0, 0));
}
#endif