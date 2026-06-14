#pragma once

#ifndef _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC
#endif
#include <crtdbg.h>
#include <cstdlib>

#ifdef _DEBUG
#undef new
#define NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define NEW new
#endif

//-----------------------------------------------------------------------------
// Windows
//-----------------------------------------------------------------------------
#include <Windows.h>

//-----------------------------------------------------------------------------
// Unnamed
//-----------------------------------------------------------------------------
#include <core/UnnamedMacro.h>
#include <core/string/StrUtil.h>
#include <engine/unnamed/subsystem/console/Log.h>
#include <core/math/Math.h>
#include <core/math/Vec2.h>
#include <core/math/Vec3.h>
#include <core/math/Vec4.h>
#include <core/math/Mat4.h>
#include <core/math/Quaternion.h>

//-----------------------------------------------------------------------------
// STD
//-----------------------------------------------------------------------------
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <malloc.h>
#include <stdexcept>
#include <string>
