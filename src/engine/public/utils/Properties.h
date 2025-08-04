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
constexpr uint32_t kMaxSrvCount = 8192; // SRVの最大数（大幅に増加）
constexpr uint32_t kSrvIndexTop = 1; // ImGuiで0番を使用するため、1番から使用

// 2Dテクスチャ用SRVの範囲
constexpr uint32_t kTexture2DStartIndex = 1;
constexpr uint32_t kTexture2DEndIndex = 4096; // 2Dテクスチャ用（1-4095）

// キューブマップテクスチャ用SRVの範囲
constexpr uint32_t kTextureCubeStartIndex = 4096;
constexpr uint32_t kTextureCubeEndIndex = 5120; // キューブマップ用（4096-5119）

// テクスチャ配列用SRVの範囲（将来の拡張用）
constexpr uint32_t kTextureArrayStartIndex = 5120;
constexpr uint32_t kTextureArrayEndIndex = 6144; // テクスチャ配列用（5120-6143）

// ストラクチャードバッファ用SRVの範囲
constexpr uint32_t kStructuredBufferStartIndex = 6144;
constexpr uint32_t kStructuredBufferEndIndex = 8192; // ストラクチャードバッファ用（6144-8191）

// 従来の共通領域（互換性のため）
constexpr uint32_t kTextureStartIndex = kTexture2DStartIndex; // 互換性のため
constexpr uint32_t kTextureEndIndex = kTexture2DEndIndex; // 互換性のため

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
constexpr uint32_t kDefaultFpsMax = 360; // フレームレート上限のデフォルト

//-----------------------------------------------------------------------------
// カメラ
//-----------------------------------------------------------------------------
constexpr float kFovMax = 179.999f * Math::deg2Rad;
constexpr float kFovMin = 0.0001f;
