#include "SequenceAssetLoader.h"

#include <filesystem>

#include "SequenceFileIO.h"
#include "core/path/PathUtil.h"
#include "core/string/StrUtil.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] bool IsSequencePath(const std::string_view path) {
			return StrUtil::ToLowerCase(std::string(path)).ends_with(
				".sequence.json"
			);
		}
	}

	bool SequenceAssetLoader::CanLoad(
		const std::string_view path,
		ASSET_TYPE* outType
	) const {
		const bool canLoad = IsSequencePath(path);
		if (outType) {
			*outType = canLoad ? ASSET_TYPE::SEQUENCE : ASSET_TYPE::UNKNOWN;
		}
		return canLoad;
	}

	LoadResult SequenceAssetLoader::Load(const std::string& path) {
		LoadResult result = {};

		SequenceFileLoadResult ioResult = {};
		if (!SequenceFileIO::LoadFromFile(path, ioResult)) {
			return result;
		}

		const std::filesystem::path filePath = Path::FromUtf8(path);
		result.payload     = std::move(ioResult.runtime);
		result.resolveName = Path::ToUtf8String(filePath.stem().stem());

		std::error_code ec;
		if (Path::ExistsUtf8(path, ec)) {
			result.stamp.sizeInBytes = Path::FileSizeUtf8(path, ec);
		}
		if (const auto lastWrite = Path::LastWriteTimeUtf8(path, ec);
			!ec) {
			result.stamp.lastWriteTicks = lastWrite.time_since_epoch().count();
		}

		return result;
	}
}
