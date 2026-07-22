#pragma once
#include <cstdint>
#include <string_view>

//-----------------------------------------------------------------------------
// Engine
//-----------------------------------------------------------------------------
static constexpr std::string_view kEngineBuildDate = __DATE__;
static constexpr std::string_view kEngineBuildTime = __TIME__;

//-----------------------------------------------------------------------------
// Window
//-----------------------------------------------------------------------------
constexpr int32_t kClientWidth  = 1280;
constexpr int32_t kClientHeight = 720;
