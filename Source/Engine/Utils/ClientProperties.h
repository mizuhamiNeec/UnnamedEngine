#pragma once

inline const wchar_t* kWindowTitle = L"MainWindow";
inline const wchar_t* kWindowClassName = L"WindowClass";
constexpr int32_t kClientWidth = 1280;
constexpr int32_t kClientHeight = 720;

constexpr uint32_t kFrameBufferCount = 2; // バックバッファの数 TODO : 起動時の引数などで変更できるようにしたい
constexpr bool kEnableVSync = true; // 垂直同期
constexpr int kMaxFPS = 256;