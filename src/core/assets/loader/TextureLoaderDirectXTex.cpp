#include "TextureLoaderDirectXTex.h"

#include <algorithm>
#include <array>
#include <DirectXTex.h>
#include <filesystem>

#include <core/assets/types/TextureAssetData.h>
#include <core/filesystem/Path.h>
#include <core/string/StrUtil.h>

#include <engine/unnamed/subsystem/console/Log.h>

namespace Unnamed {
	static constexpr std::string_view kChannel = "TxLdrDtx";

	namespace {
		[[nodiscard]] bool IsSrgbFormat(const DXGI_FORMAT format) {
			switch (format) {
				case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
				case DXGI_FORMAT_BC1_UNORM_SRGB:
				case DXGI_FORMAT_BC2_UNORM_SRGB:
				case DXGI_FORMAT_BC3_UNORM_SRGB:
				case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
				case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
				case DXGI_FORMAT_BC7_UNORM_SRGB: return true;
				default: return false;
			}
		}
	}

	static constexpr std::array kSupportedExtensions = {
		// DirectDraw Surface
		".dds",

		// HDR
		".hdr",

		// TGA
		".tga",

		// Windows Imaging Component
		".bmp",
		".png",
		".gif",
		".tif",
		".tiff",
		".jpg",
		".jpeg",
	};

	bool TextureLoaderDirectXTex::CanLoad(
		const Path& path, ASSET_TYPE* outType
	) const {
		bool ok = false;
		const std::string pathExt =
			StrUtil::ToLowerCase(path.Extension().ToGenericUtf8());

		// サポートされている拡張子か確認
		for (const auto& supported : kSupportedExtensions) {
			if (pathExt == supported) {
				ok = true;
				break;
			}
		}

		// 出力タイプの設定
		if (outType) {
			if (ok) {
				// テクスチャとして認識
				*outType = ASSET_TYPE::TEXTURE;
			} else {
				// 不明
				*outType = ASSET_TYPE::UNKNOWN;
			}
		}

		return ok;
	}

	LoadResult TextureLoaderDirectXTex::Load(const Path& path) {
		LoadResult r = {};
		using namespace DirectX;

		ScratchImage img;
		TexMetadata  meta = {};
		HRESULT      hr   = E_FAIL;

		std::wstring wPath = path.Native().wstring();
		std::string  ext   = StrUtil::ToLowerCase(
			path.Extension().ToGenericUtf8()
		);

		if (ext == ".dds") {
			hr = LoadFromDDSFile(
				wPath.c_str(), DDS_FLAGS_NONE, &meta, img
			);
		} else if (ext == ".hdr") {
			hr = LoadFromHDRFile(
				wPath.c_str(), &meta, img
			);
		} else if (ext == ".tga") {
			hr = LoadFromTGAFile(
				wPath.c_str(), &meta, img
			);
		} else {
			hr = LoadFromWICFile(
				wPath.c_str(), WIC_FLAGS_NONE, &meta, img
			);
		}

		if (FAILED(hr)) {
			Error(kChannel, "テクスチャの読み込みに失敗しました: {}", path);
			return r;
		}

		// ミップマップがない場合は生成する
		if (meta.mipLevels <= 1) {
			Msg(kChannel, "ミップマップが存在しないため生成します。: {}", path);
			ScratchImage chain;
			hr = GenerateMipMaps(
				img.GetImages(), img.GetImageCount(), meta,
				TEX_FILTER_CUBIC, 0, chain
			);
			if (SUCCEEDED(hr)) {
				img  = std::move(chain);
				meta = img.GetMetadata();
				SpecialMsg(
					LogLevel::Success, kChannel,
					"{} 段階のミップマップを生成しました。", meta.mipLevels
				);
			} else {
				Warning(
					kChannel, "ミップマップ生成に失敗しました: {}。そのまま使用します。", path
				);
			}
		}

		// アセットデータにつめつめ
		TextureAssetData out = {};
		out.width     = static_cast<uint32_t>(meta.width);
		out.height    = static_cast<uint32_t>(meta.height);
		out.arraySize = static_cast<uint32_t>(std::max<size_t>(meta.arraySize, 1));
		out.mipLevels = static_cast<uint32_t>(std::max<size_t>(meta.mipLevels, 1));
		out.format     = meta.format;
		out.isSRGB     = IsSrgbFormat(meta.format);
		out.isCubeMap  = meta.IsCubemap();
		out.dimension  = out.isCubeMap ?
			                 TEXTURE_DIMENSION::TEXTURE_CUBE :
			                 TEXTURE_DIMENSION::TEXTURE_2D;
		out.sourcePath = path.LexicallyNormal();

		out.subresources.reserve(
			static_cast<size_t>(out.arraySize) * static_cast<size_t>(out.mipLevels)
		);
		out.mips.resize(out.mipLevels);

		for (size_t arraySlice = 0; arraySlice < out.arraySize; ++arraySlice) {
			for (size_t mipLevel = 0; mipLevel < out.mipLevels; ++mipLevel) {
				const Image* im = img.GetImage(mipLevel, arraySlice, 0);
				if (!im || !im->pixels) {
					continue;
				}

				TextureSubresource subresource = {};
				subresource.width = static_cast<uint32_t>(im->width);
				subresource.height = static_cast<uint32_t>(im->height);
				subresource.rowPitch = static_cast<size_t>(im->rowPitch);
				subresource.slicePitch = static_cast<size_t>(im->slicePitch);
				subresource.mipLevel = static_cast<uint32_t>(mipLevel);
				subresource.arraySlice = static_cast<uint32_t>(arraySlice);
				subresource.bytes.resize(subresource.slicePitch);
				memcpy(
					subresource.bytes.data(), im->pixels, subresource.slicePitch
				);

				if (arraySlice == 0) {
					TextureMip mip = {};
					mip.width      = subresource.width;
					mip.height     = subresource.height;
					mip.rowPitch   = subresource.rowPitch;
					mip.bytes      = subresource.bytes;
					out.mips[mipLevel] = std::move(mip);
				}

				out.subresources.emplace_back(std::move(subresource));
			}
		}

		r.payload     = std::move(out);
		r.resolveName = Path::ToUtf8String(path.FileName());
		if (std::error_code ec; std::filesystem::exists(path.Native(), ec)) {
			r.stamp.sizeInBytes = std::filesystem::file_size(
				path.Native(), ec
			);
		}

		return r;
	}
}
