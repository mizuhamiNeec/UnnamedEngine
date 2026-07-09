#pragma once
#include <cstdint>
#include <vector>

#include <dxgiformat.h>

#include "core/filesystem/Path.h"

namespace Unnamed {
	enum class TEXTURE_DIMENSION : uint8_t {
		TEXTURE_2D   = 0,
		TEXTURE_CUBE = 1,
	};

	// @brief テクスチャのミップマップ情報
	struct TextureMip {
		std::vector<uint8_t> bytes;        // ピクセルデータ
		uint32_t             width    = 0; // ミップマップの幅
		uint32_t             height   = 0; // ミップマップの高さ
		size_t               rowPitch = 0; // 1行あたりのバイト数
	};

	// @brief テクスチャサブリソース情報（mip + arraySlice）
	struct TextureSubresource {
		std::vector<uint8_t> bytes;
		uint32_t             width      = 0;
		uint32_t             height     = 0;
		size_t               rowPitch   = 0;
		size_t               slicePitch = 0;
		uint32_t             mipLevel   = 0;
		uint32_t             arraySlice = 0;
	};

	/// @brief テクスチャアセットのデータ構造体
	struct TextureAssetData {
		std::vector<uint8_t> bytes; // 元のファイルデータ
		std::vector<TextureMip> mips; // 互換用途(2D mip only)
		std::vector<TextureSubresource> subresources; // 汎用サブリソース
		uint32_t width = 0; // テクスチャの幅
		uint32_t height = 0; // テクスチャの高さ
		uint32_t arraySize = 1;
		uint32_t mipLevels = 0;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		bool isSRGB = false; // sRGBかどうか
		bool isCubeMap = false;
		TEXTURE_DIMENSION dimension = TEXTURE_DIMENSION::TEXTURE_2D;
		Path sourcePath; // ソースファイルパス
	};
}
