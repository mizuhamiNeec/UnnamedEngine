#pragma once

#include <Lib/Math/MathLib.h>
#include <dxgiformat.h>

//-----------------------------------------------------------------------------
// Purpose: アプリケーション
//-----------------------------------------------------------------------------
const std::string kAppName = "Unnamed Application";

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
// Purpose : UI
//-----------------------------------------------------------------------------
constexpr float kTitleBarH = 24.0f;
constexpr float kTopToolbarH = 35.0f;
constexpr float kStatusBarH = 35.0f;

//-----------------------------------------------------------------------------
// Purpose : レンダラ
//-----------------------------------------------------------------------------
constexpr uint32_t kFrameBufferCount = 2;    // バックバッファの数 TODO: 引数などで変更
constexpr uint32_t kMaxRenderTargetCount = 16;   // レンダーターゲットの最大数
constexpr uint32_t kMaxSrvCount = 2048; // SRVの最大数

// SRVの種類別インデックス範囲
constexpr uint32_t kSrvIndexTop = 1; // ImGuiで0番を使用するため、1番から使用
constexpr uint32_t kTextureStartIndex = 1;       // テクスチャ用SRVの開始インデックス
constexpr uint32_t kTextureEndIndex = 1536;      // テクスチャ用SRVの終了インデックス（1535まで使用可）
constexpr uint32_t kStructuredBufferStartIndex = 1536; // ストラクチャードバッファ用SRVの開始インデックス
constexpr uint32_t kStructuredBufferEndIndex = 2048;   // ストラクチャードバッファ用SRVの終了インデックス（2047まで使用可）

// 以前の定義を削除
//constexpr uint32_t kSrvIndexTop = 1; // ImGuiで0番を使用するため、1番から使用
constexpr uint32_t kMaxFps = 360;  // フレームレートの上限

// バッファのフォーマット
//constexpr DXGI_FORMAT kBufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
constexpr DXGI_FORMAT kBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // 8bit RGBA

//-----------------------------------------------------------------------------
// Purpose: カメラ
//-----------------------------------------------------------------------------
constexpr float kFovMax = 179.999f * Math::deg2Rad;
constexpr float kFovMin = 0.0001f;

//-----------------------------------------------------------------------------
// Purpose : テクスチャマネージャー
//-----------------------------------------------------------------------------
// kSrvIndexTop は上部で定義済み（重複削除）
