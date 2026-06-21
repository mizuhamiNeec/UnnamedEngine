#include "SequenceAssetLoader.h"
#include "core/filesystem/Path.h"

#include <filesystem>

#include "SequenceFileIO.h"

#include "core/string/StrUtil.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] bool IsSequencePath(const Path& path) {
			return StrUtil::ToLowerCase(path.ToGenericUtf8()).ends_with(
				".sequence.json"
			);
		}
	}

	bool SequenceAssetLoader::CanLoad(
		const Path& path,
		ASSET_TYPE* outType
	) const {
		const bool canLoad = IsSequencePath(path);
		if (outType) {
			*outType = canLoad ? ASSET_TYPE::SEQUENCE : ASSET_TYPE::UNKNOWN;
		}
		return canLoad;
	}

	LoadResult SequenceAssetLoader::Load(const Path& path) {
		LoadResult result = {};

		SequenceFileLoadResult ioResult = {};
		if (!SequenceFileIO::LoadFromFile(path, ioResult)) {
			return result;
		}

		const Path filePath = path.LexicallyNormal();
		result.payload     = std::move(ioResult.runtime);
		result.resolveName = Path::ToUtf8String(filePath.Stem().Stem());

		std::error_code ec;
		if (std::filesystem::exists(path.Native(), ec)) {
			result.stamp.sizeInBytes = std::filesystem::file_size(
				path.Native(), ec
			);
		}
		if (const auto lastWrite = std::filesystem::last_write_time(
			    path.Native(), ec
		    );
			!ec) {
			result.stamp.lastWriteTicks = lastWrite.time_since_epoch().count();
		}

		return result;
	}
}
