#pragma once

#ifndef _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC
#endif
#include <crtdbg.h>
#include <cstdlib>

#ifdef _DEBUG
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
#include <engine/subsystem/console/Log.h>
#include <core/UnnamedMacro.h>
#include <core/string/StrUtil.h>

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
