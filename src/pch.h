#pragma once

#ifndef _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC
#endif
#include <cstdlib>
#include <crtdbg.h>

#ifdef _DEBUG
#define NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define NEW new
#endif

//-----------------------------------------------------------------------------
// Windows
//-----------------------------------------------------------------------------
#define NOMINMAX
#include <Windows.h>

//-----------------------------------------------------------------------------
// Unnamed
//-----------------------------------------------------------------------------
#include <engine/public/utils/StrUtil.h>
#include <engine/public/utils/UnnamedMacro.h>

//-----------------------------------------------------------------------------
// STD
//-----------------------------------------------------------------------------
#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <malloc.h>
