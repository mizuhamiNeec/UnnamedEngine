#pragma once
#include <cstdint>

namespace Unnamed::Rhi {
	/// @brief DeviceDescは、D3D12 device生成時のadapter選択とdebug機能設定を指定します
	struct DeviceDesc {
		bool enableDebugLayer         = false; // デバッグレイヤーを有効にするか?
		bool enableGpuBasedValidation = false; // バリデーションレイヤーを有効にするか?
	};

	enum class TEXTURE_FORMAT : uint8_t {
		R8G8B8A8_UNORM,   // 32-bit RGBA
		R10G10B10A2_UNORM // 10-10-10-2 RGBA
	};

	/// @brief SwapChainDescは、swap chainのwindow、幅、高さ、format、buffer数を指定します
	struct SwapChainDesc {
		uint32_t width       = 1280; // デフォルト幅
		uint32_t height      = 720;  // デフォルト高さ
		uint32_t bufferCount = 2;    // ダブルバッファリング

		// バックバッファのフォーマット
		TEXTURE_FORMAT format = TEXTURE_FORMAT::R8G8B8A8_UNORM;

		bool vSync = false; // 垂直同期を行うか?
	};

	/// @brief ClearColorは、描画色のRGBA成分を同一色空間の値として保持します
	struct ClearColor {
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
		float a = 1.0f;
	};
}
