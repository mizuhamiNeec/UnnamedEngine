#ifdef _DEBUG
#include <engine/ImGui/ImGuiUtil.h>
#include <runtime/core/math/Math.h>

/// @brief Vec4型をImVec4型に変換します。
/// @param vec 変換するVec4型のベクトル
/// @return 変換後のImVec4型のベクトル
ImVec4 ImGuiUtil::ToImVec4(const Vec4& vec) {
	return {vec.x, vec.y, vec.z, vec.w};
}
#endif
