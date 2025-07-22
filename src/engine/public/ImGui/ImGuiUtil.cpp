#ifdef _DEBUG
#include <engine/public/ImGui/ImGuiUtil.h>

#include <math/public/Vec4.h>

ImVec4 ImGuiUtil::ToImVec4(const Vec4& vec) {
	return {vec.x, vec.y, vec.z, vec.w};
}
#endif
