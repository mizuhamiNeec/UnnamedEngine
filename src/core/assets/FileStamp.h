#pragma once
#include <cstdint>
#include <filesystem>

#include "core/filesystem/Path.h"

namespace Unnamed {
	/// @brief ファイル更新検知に使う更新時刻とサイズのスナップショットを保持します
	struct FileStamp {
		int64_t  lastWriteTicks = 0;
		uint64_t sizeInBytes    = 0;
	};

	/// @brief 現在のファイル状態をFileStampとして読み取ります。
	/// @param path 対象ファイルのパス
	/// @return ファイルが存在しない、または状態を取得できない場合は空のFileStamp
	[[nodiscard]] inline FileStamp ReadCurrentFileStamp(const Path& path) {
		FileStamp stamp = {};
		if (path.IsEmpty()) {
			return stamp;
		}

		std::error_code ec;
		if (!std::filesystem::exists(path.Native(), ec)) {
			return stamp;
		}

		const auto lastWrite = std::filesystem::last_write_time(
			path.Native(), ec
		);
		if (!ec) {
			stamp.lastWriteTicks = lastWrite.time_since_epoch().count();
		}

		std::error_code fileEc;
		if (std::filesystem::is_regular_file(path.Native(), fileEc)) {
			const auto size = std::filesystem::file_size(
				path.Native(), fileEc
			);
			if (!fileEc) {
				stamp.sizeInBytes = size;
			}
		}

		return stamp;
	}

	/// @brief 不完全なFileStampを現在のファイル状態で補完します。
	/// @param path 補完に使うファイルパス
	/// @param partialStamp 補完前のFileStamp
	/// @return sizeInBytesまたはlastWriteTicksが0の場合だけ現在値で補完したFileStamp
	[[nodiscard]] inline FileStamp CompleteFileStamp(
		const Path& path, const FileStamp& partialStamp
	) {
		FileStamp completed = partialStamp;
		if (
			completed.sizeInBytes != 0 &&
			completed.lastWriteTicks != 0
		) {
			return completed;
		}

		const FileStamp current = ReadCurrentFileStamp(path);
		if (completed.sizeInBytes == 0) {
			completed.sizeInBytes = current.sizeInBytes;
		}
		if (completed.lastWriteTicks == 0) {
			completed.lastWriteTicks = current.lastWriteTicks;
		}
		return completed;
	}

	/// @brief 2つのFileStampが同じファイル状態を表すかを判定します。
	/// @param lhs 比較するFileStamp
	/// @param rhs 比較するFileStamp
	/// @return 更新日時tickとファイルサイズが一致する場合はtrue
	[[nodiscard]] inline bool FileStampEquals(
		const FileStamp& lhs, const FileStamp& rhs
	) {
		return lhs.lastWriteTicks == rhs.lastWriteTicks &&
		       lhs.sizeInBytes == rhs.sizeInBytes;
	}
}
