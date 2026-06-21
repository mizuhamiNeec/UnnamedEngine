#include "SoundAssetLoader.h"
#include "core/filesystem/Path.h"

#include <array>
#include <filesystem>
#include <fstream>

#include "core/assets/types/SoundAssetData.h"

#include "core/string/StrUtil.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] uint16_t ReadLe16(const uint8_t* data) {
			return static_cast<uint16_t>(data[0]) |
			       static_cast<uint16_t>(data[1] << 8);
		}

		[[nodiscard]] uint32_t ReadLe32(const uint8_t* data) {
			return static_cast<uint32_t>(data[0]) |
			       static_cast<uint32_t>(data[1] << 8) |
			       static_cast<uint32_t>(data[2] << 16) |
			       static_cast<uint32_t>(data[3] << 24);
		}

		bool ReadExact(
			std::ifstream& file, void* dst, const std::streamsize byteCount
		) {
			file.read(static_cast<char*>(dst), byteCount);
			return file.good();
		}
	}

	bool SoundAssetLoader::CanLoad(
		const Path& path, ASSET_TYPE* outType
	) const {
		const bool ok = StrUtil::ToLowerCase(path.ToGenericUtf8()).ends_with(
			".wav");
		if (outType) {
			*outType = ok ? ASSET_TYPE::SOUND : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult SoundAssetLoader::Load(const Path& path) {
		LoadResult    result = {};
		std::ifstream file(path.Native(), std::ios::binary);
		if (!file.is_open()) {
			return result;
		}

		std::array<uint8_t, 12> riffHeader = {};
		if (!ReadExact(
			file,
			riffHeader.data(),
			static_cast<std::streamsize>(riffHeader.size())
		)) {
			return result;
		}

		const bool hasRiff = riffHeader[0] == 'R' && riffHeader[1] == 'I' &&
		                     riffHeader[2] == 'F' && riffHeader[3] == 'F';
		const bool hasWave = riffHeader[8] == 'W' && riffHeader[9] == 'A' &&
		                     riffHeader[10] == 'V' && riffHeader[11] == 'E';
		if (!hasRiff || !hasWave) {
			return result;
		}

		SoundAssetData soundData = {};
		bool           foundFmt  = false;
		bool           foundData = false;

		while (file.good() && !foundData) {
			std::array<uint8_t, 8> chunkHeader = {};
			if (!ReadExact(
				file,
				chunkHeader.data(),
				static_cast<std::streamsize>(chunkHeader.size())
			)) {
				break;
			}

			const uint32_t chunkSize = ReadLe32(chunkHeader.data() + 4);
			const bool     isFmt     =
				chunkHeader[0] == 'f' && chunkHeader[1] == 'm' &&
				chunkHeader[2] == 't' && chunkHeader[3] == ' ';
			const bool isData =
				chunkHeader[0] == 'd' && chunkHeader[1] == 'a' &&
				chunkHeader[2] == 't' && chunkHeader[3] == 'a';

			if (isFmt) {
				if (chunkSize < 16) {
					return {};
				}

				std::vector<uint8_t> fmtBytes(chunkSize);
				if (!ReadExact(
					file,
					fmtBytes.data(),
					static_cast<std::streamsize>(chunkSize)
				)) {
					return {};
				}

				soundData.formatTag = ReadLe16(fmtBytes.data() + 0);
				soundData.channels = ReadLe16(fmtBytes.data() + 2);
				soundData.sampleRate = ReadLe32(fmtBytes.data() + 4);
				soundData.averageBytesPerSecond = ReadLe32(fmtBytes.data() + 8);
				soundData.blockAlign = ReadLe16(fmtBytes.data() + 12);
				soundData.bitsPerSample = ReadLe16(fmtBytes.data() + 14);
				foundFmt = true;
			} else if (isData) {
				if (chunkSize == 0) {
					return {};
				}
				soundData.pcmData.resize(chunkSize);
				if (!ReadExact(
					file,
					soundData.pcmData.data(),
					static_cast<std::streamsize>(chunkSize)
				)) {
					return {};
				}
				foundData = true;
			} else {
				file.seekg(static_cast<std::streamoff>(chunkSize),
				           std::ios::cur);
			}

			if ((chunkSize & 1u) != 0u) {
				file.seekg(1, std::ios::cur);
			}
		}

		if (
			!foundFmt || !foundData ||
			soundData.formatTag != 1 ||
			soundData.channels == 0 ||
			soundData.sampleRate == 0 ||
			soundData.bitsPerSample == 0 ||
			soundData.pcmData.empty()
		) {
			return {};
		}

		const Path full    = path.LexicallyNormal();
		result.payload     = std::move(soundData);
		result.resolveName = Path::ToUtf8String(full.Stem());

		std::error_code ec;
		if (std::filesystem::exists(path.Native(), ec)) {
			result.stamp.sizeInBytes = std::filesystem::file_size(
				path.Native(), ec
			);
		}
		return result;
	}
}
