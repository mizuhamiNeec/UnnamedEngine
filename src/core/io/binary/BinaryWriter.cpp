#include "BinaryWriter.h"

#include "core/path/PathUtil.h"

namespace Unnamed {
	BinaryWriter::BinaryWriter(const std::string& path) : mStream(
		Path::FromUtf8(path), std::ios::binary | std::ios::trunc
	) {}

	bool BinaryWriter::IsOpen() const {
		return mStream.is_open();
	}

	bool BinaryWriter::Good() const {
		return static_cast<bool>(mStream);
	}

	uint64_t BinaryWriter::Tell() {
		return mStream.tellp();
	}

	bool BinaryWriter::Seek(const uint64_t offset) {
		mStream.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
		return Good();
	}

	bool BinaryWriter::WriteBytes(const void* data, const size_t sizeBytes) {
		if (sizeBytes == 0) {
			return true;
		}
		if (data == nullptr) {
			return false;
		}
		mStream.write(
			static_cast<const char*>(data),
			static_cast<std::streamsize>(sizeBytes)
		);
		return Good();
	}

	bool BinaryWriter::WriteString(const std::string_view text) {
		if (text.size() > static_cast<size_t>(std::numeric_limits<
			    uint32_t>::max())) {
			return false;
		}
		const uint32_t sizeBytes = static_cast<uint32_t>(text.size());
		return WritePod(sizeBytes) &&
		       WriteBytes(text.data(), sizeBytes);
	}

	bool BinaryWriter::Flush() {
		mStream.flush();
		return Good();
	}
}
