#pragma once
#include <cstdint>
#include <dxgiformat.h>
#include <string>

//-----------------------------------------------------------------------------
// Engine
//-----------------------------------------------------------------------------
const std::string kEngineBuildDate = __DATE__;
const std::string kEngineBuildTime = __TIME__;

//-----------------------------------------------------------------------------
// Window
//-----------------------------------------------------------------------------
constexpr int32_t kClientWidth  = 1280;
constexpr int32_t kClientHeight = 720;

//-----------------------------------------------------------------------------
// UI
//-----------------------------------------------------------------------------
constexpr float kTitleBarH   = 24.0f;
constexpr float kTopToolbarH = 35.0f;
constexpr float kStatusBarH  = 35.0f;

//-----------------------------------------------------------------------------
// Renderer
//-----------------------------------------------------------------------------
constexpr uint32_t kFrameBufferCount = 2;
constexpr uint32_t kMaxRenderTargetCount = 16; // レンダーターゲットの最大数
constexpr uint32_t kMaxSrvCount = 2048; // SRVの最大数
constexpr uint32_t kSrvIndexTop = 1; // ImGuiで0番を使用するため、1番から使用
constexpr uint32_t kTextureStartIndex = 1; // テクスチャ用SRVの開始インデックス
constexpr uint32_t kTextureEndIndex = 1536; // テクスチャ用SRVの終了インデックス（1535まで使用可）

// ストラクチャードバッファ用SRVの開始インデックス
constexpr uint32_t kStructuredBufferStartIndex = 1536;
// ストラクチャードバッファ用SRVの終了インデックス（2047まで使用可）
constexpr uint32_t kStructuredBufferEndIndex = 2048;

constexpr DXGI_FORMAT kBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

//-----------------------------------------------------------------------------
// DescriptorHeap
//-----------------------------------------------------------------------------
constexpr uint32_t kRtvHeapCount     = 64;
constexpr uint32_t kDsvHeapCount     = 32;
constexpr uint32_t kSrvHeapCount     = 4096;
constexpr uint32_t kSamplerHeapCount = 64;

//-----------------------------------------------------------------------------
// Graphics
//-----------------------------------------------------------------------------
constexpr bool     kEnableFrameRateLimit = true; // フレームレート制限を有効にするか
constexpr uint32_t kFpsMax               = 360;  // フレームレートの上限


//-----------------------------------------------------------------------------
// カメラ
//-----------------------------------------------------------------------------
constexpr float kFovMax = 179.999f * Math::deg2Rad;
constexpr float kFovMin = 0.0001f;
