#pragma once
#include "Math/MathLib.h"

//-----------------------------------------------------------------------------
// Purpose : ウィンドウ
//-----------------------------------------------------------------------------
const std::string kWindowTitle = "MainWindow";
const std::string kWindowClassName = "WindowClass";
constexpr int32_t kClientWidth = 1280;
constexpr int32_t kClientHeight = 720;

//-----------------------------------------------------------------------------
// Purpose : レンダラ
//-----------------------------------------------------------------------------
constexpr uint32_t kFrameBufferCount = 2; // バックバッファの数 TODO: 引数などで変更
constexpr uint32_t kMaxSrvCount = 512;
constexpr bool kEnableVerticalSync = false; // 垂直同期
constexpr float kMaxFPS = 256.0f;

//-----------------------------------------------------------------------------
// Purpose: カメラ
//-----------------------------------------------------------------------------
constexpr float kFovMax = 179.999f * Math::deg2Rad;
constexpr float kFovMin = 0.0001f;

//-----------------------------------------------------------------------------
// Purpose : テクスチャマネージャー
//-----------------------------------------------------------------------------
constexpr uint32_t kSrvIndexTop = 1; // ImGuiで0番を使用するため、1番から使用

//-----------------------------------------------------------------------------
// Purpose : コンソール
//-----------------------------------------------------------------------------
constexpr int kConsoleMaxLineCount = 512;
