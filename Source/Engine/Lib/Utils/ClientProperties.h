#pragma once

#include <Lib/Math/MathLib.h>

//-----------------------------------------------------------------------------
// Purpose: エンジン
//-----------------------------------------------------------------------------
const std::string kEngineName = "Unnamed Engine";
const std::string kEngineVersion = "0.1.2";
const std::string kEngineBuildDate = __DATE__;
const std::string kEngineBuildTime = __TIME__;

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
constexpr uint32_t kMaxSrvCount = 1024; // SRVの最大数
constexpr uint32_t kMaxFps = 360; // フレームレートの上限

//-----------------------------------------------------------------------------
// Purpose: カメラ
//-----------------------------------------------------------------------------
constexpr float kFovMax = 179.999f * Math::deg2Rad;
constexpr float kFovMin = 0.0001f;

//-----------------------------------------------------------------------------
// Purpose : テクスチャマネージャー
//-----------------------------------------------------------------------------
constexpr uint32_t kSrvIndexTop = 1; // ImGuiで0番を使用するため、1番から使用
