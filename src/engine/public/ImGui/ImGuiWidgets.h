#pragma once
#include <string>

#ifdef _DEBUG
#include <imgui.h>

namespace ImGuiWidgets {
	void PushStyleColorForDrag(
		const ImVec4& bg,
		const ImVec4& bgHovered,
		const ImVec4& bgActive
	);
	
	bool DragVec3(
		const std::string& name,
		Vec3&              value,
		const Vec3&        defaultValue,
		const float&       vSpeed,
		const char*        format
	);
	
	bool EditCubicBezier(
		const std::string& label,
		float&             p0,
		float&             p1,
		float&             p2,
		float&             p3
	);
	
	bool IconButton(
		const char* icon,
		const char* label     = nullptr,
		ImVec2      size      = ImVec2(0, 0),
		float       iconScale = 0.40f,
		ImGuiDir    labelDir  = ImGuiDir_Down
	);

	bool MenuItemWithIcon(const char* icon, const char* label);
	bool BeginMainMenu(const char* label, bool enabled = true);
}
#endif
