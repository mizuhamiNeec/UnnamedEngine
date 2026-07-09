#include "BinaryReader.h"

#include "core/filesystem/Path.h"

namespace Unnamed {
	BinaryReader::BinaryReader(const Path& path) : mStream(
		path.Native(), std::ios::binary
	) {
		if (!IsOpen()) {
			return;
		}
		mStream.seekg(0, std::ios::end);
		mFileSize = static_cast<uint64_t>(mStream.tellg());
		mStream.seekg(0, std::ios::beg);
	}

	bool BinaryReader::IsOpen() const {
		return mStream.is_open();
	}

	bool BinaryReader::Good() const {
		return static_cast<bool>(mStream);
	}

	uint64_t BinaryReader::Tell() {
		return mStream.tellg();
	}

	bool BinaryReader::Seek(const uint64_t offset) {
		mStream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
		return Good();
	}

	bool BinaryReader::Skip(const uint64_t bytes) {
		mStream.seekg(static_cast<std::streamoff>(bytes), std::ios::cur);
		return Good();
	}

	uint64_t BinaryReader::Remaining() {
		const uint64_t cursor = Tell();
		return cursor <= mFileSize ? mFileSize - cursor : 0ull;
	}

	bool BinaryReader::ReadBytes(void* outData, const size_t sizeBytes) {
		if (sizeBytes == 0) {
			return true;
		}
		if (outData == nullptr) {
			return false;
		}
		if (Remaining() < sizeBytes) {
			return false;
		}
		mStream.read(
			static_cast<char*>(outData),
			static_cast<std::streamsize>(sizeBytes)
		);
		return Good();
	}

	bool BinaryReader::ReadString(std::string& outText) {
		uint32_t sizeBytes = 0;
		if (!ReadPod(sizeBytes)) {
			return false;
		}
		if (Remaining() < sizeBytes) {
			return false;
		}
		outText.resize(sizeBytes);
		return ReadBytes(outText.data(), sizeBytes);
	}
}
