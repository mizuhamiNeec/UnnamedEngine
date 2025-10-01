#include <pch.h>

//-----------------------------------------------------------------------------

#include "RawLoader.h"

#include <filesystem>
#include <fstream>

namespace Unnamed {
	static std::string ReadTextFile(const std::string& path) {
		const std::ifstream ifs(path, std::ios::binary);
		if (!ifs) {
			return {};
		}
		std::stringstream ss;
		ss << ifs.rdbuf();
		return ss.str();
	}

	bool RawLoader::CanLoad(
		const std::string_view path,
		UASSET_TYPE*           outType
	) const {
		const std::string ext = StrUtil::ToLowerExt(path);
		const bool        ok  = (
			ext == ".txt" ||
			ext == ".json" ||
			ext == ".cfg" ||
			ext == ".ini" ||
			ext == ".hlsl"
		);
		if (outType) {
			*outType = ok ? UASSET_TYPE::RAWFILE : UASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult RawLoader::Load(const std::string& path) {
		LoadResult result = {};

		const std::string ext = StrUtil::ToLowerExt(path);

		RawFileAssetData data = {};
		data.sourcePath       = path;

		data.raw = ReadTextFile(path);

		result.resolveName = std::filesystem::path(path).filename().string();
		result.stamp.sizeInBytes = std::filesystem::file_size(path);

		result.payload = data;
		return result;
	}
}
