#include "Vec4.h"

Vec4 Vec4::one = {.x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f};
Vec4 Vec4::zero = {.x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 0.0f};

Vec4 Vec4::red = {.x = 1.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f};
Vec4 Vec4::green = {.x = 0.0f, .y = 1.0f, .z = 0.0f, .w = 1.0f};
Vec4 Vec4::blue = {.x = 0.0f, .y = 0.0f, .z = 1.0f, .w = 1.0f};
Vec4 Vec4::white = {.x = 1.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f};
Vec4 Vec4::black = {.x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f};
Vec4 Vec4::yellow = {.x = 1.0f, .y = 1.0f, .z = 0.0f, .w = 1.0f};
Vec4 Vec4::cyan = {.x = 0.0f, .y = 1.0f, .z = 1.0f, .w = 1.0f};
Vec4 Vec4::magenta = {.x = 1.0f, .y = 0.0f, .z = 1.0f, .w = 1.0f};
Vec4 Vec4::gray = {.x = 0.5f, .y = 0.5f, .z = 0.5f, .w = 1.0f};
Vec4 Vec4::lightGray = {.x = 0.75f, .y = 0.75f, .z = 0.75f, .w = 1.0f};
Vec4 Vec4::darkGray = {.x = 0.25f, .y = 0.25f, .z = 0.25f, .w = 1.0f};
Vec4 Vec4::orange = {.x = 1.0f, .y = 0.5f, .z = 0.0f, .w = 1.0f};
Vec4 Vec4::purple = {.x = 0.5f, .y = 0.0f, .z = 0.5f, .w = 1.0f};
Vec4 Vec4::brown = {.x = 0.6f, .y = 0.3f, .z = 0.0f, .w = 1.0f};

#ifdef _DEBUG
ImVec4 ToImVec4(const Vec4& vec) {
	return {vec.x, vec.y, vec.z, vec.w};
}
#endif
