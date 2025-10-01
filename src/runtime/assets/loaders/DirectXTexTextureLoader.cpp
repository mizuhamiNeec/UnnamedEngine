#include <pch.h>

//-----------------------------------------------------------------------------

#include "DirectXTexTextureLoader.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <DirectXTex.h>
#include <filesystem>

namespace Unnamed {
	constexpr std::string_view kChannel = "DirectXTexTextureLoader";

	bool DirectXTexTextureLoader::CanLoad(
		std::string_view path,
		UASSET_TYPE*     outType
	) const {
		std::string ext = StrUtil::ToLowerExt(path);
		bool        ok  = (
			ext == ".dds" ||
			ext == ".png" ||
			ext == ".jpg" ||
			ext == ".jpeg" ||
			ext == ".tga" ||
			ext == ".bmp" ||
			ext == ".hdr"
		);
		if (outType) {
			*outType = ok ? UASSET_TYPE::TEXTURE : UASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult DirectXTexTextureLoader::Load(
		const std::string& path
	) {
		LoadResult r = {};
		using namespace DirectX;

		ScratchImage img;
		TexMetadata  meta = {};
		HRESULT      hr   = E_FAIL;

		std::wstring wPath = StrUtil::ToWString(path);
		std::string  ext   = StrUtil::ToLowerExt(path);
		if (ext == ".dds") {
			hr = LoadFromDDSFile(
				wPath.c_str(), DDS_FLAGS_NONE, &meta, img
			);
		} else {
			hr = LoadFromWICFile(
				wPath.c_str(), WIC_FLAGS_NONE, &meta, img
			);
		}
		if (FAILED(hr)) {
			Error(kChannel, "Failed to load texture: {}", path);
			return r;
		}

		// RGBAへ変換
		constexpr DXGI_FORMAT kTarget = DXGI_FORMAT_R8G8B8A8_UNORM;
		ScratchImage          converted;
		const Image*          base = nullptr;
		if (meta.format != kTarget) {
			hr = Convert(
				img.GetImages(),
				img.GetImageCount(),
				meta,
				kTarget,
				TEX_FILTER_DEFAULT,
				0.0,
				converted
			);
			if (FAILED(hr)) {
				Error(kChannel, "Failed to convert texture: {}", path);
				return r;
			}
			base = converted.GetImage(0, 0, 0);
			meta = converted.GetMetadata();
			img  = std::move(converted);
		}

		// ミップマップがない場合は生成
		if (meta.mipLevels <= 1) {
			ScratchImage chain;
			hr = GenerateMipMaps(
				img.GetImages(), img.GetImageCount(), meta,
				TEX_FILTER_CUBIC, 0, chain
			);

			if (SUCCEEDED(hr)) {
				img  = std::move(chain);
				meta = img.GetMetadata();
			}
		}

		TextureAssetData out = {};
		out.width            = static_cast<uint32_t>(meta.width);
		out.height           = static_cast<uint32_t>(meta.height);
		out.sourcePath       = path;

		out.mips.resize(meta.mipLevels);
		for (size_t m = 0; m < meta.mipLevels; ++m) {
			const Image* im  = img.GetImage(m, 0, 0);
			TextureMip   mip = {};
			mip.width        = static_cast<uint32_t>(im->width);
			mip.height       = static_cast<uint32_t>(im->height);
			mip.rowPitch     = size_t(mip.width) * 4; // RGBA8
			mip.bytes.resize(mip.rowPitch * mip.height);

			for (uint32_t y = 0; y < mip.height; ++y) {
				memcpy(
					mip.bytes.data() + y * mip.rowPitch,
					im->pixels + y * im->rowPitch,
					std::min(mip.rowPitch,
					         static_cast<size_t>(im->rowPitch)
					)
				);
			}
			out.mips[m] = std::move(mip);
		}

		r.payload     = std::move(out);
		r.resolveName = std::filesystem::path(path).filename().string();
		if (std::error_code ec; std::filesystem::exists(path, ec)) {
			r.stamp.sizeInBytes = std::filesystem::file_size(path, ec);
		}

		return r;
	}
}
