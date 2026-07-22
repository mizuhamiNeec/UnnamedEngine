#pragma once

#include <string>
#include <string_view>

#include "DemoTypes.h"

#include "core/io/binary/BinaryReader.h"

namespace Unnamed {
	class DemoBinaryReader {
	public:
		[[nodiscard]] static bool ReadFile(
			const Path&  path,
			DemoFileV2&  outFile,
			std::string* outError = nullptr
		);

	private:
		static void SetError(
			std::string* outError, std::string_view text
		);

		[[nodiscard]] static bool ReadHeadChunk(
			BinaryReader& reader, DemoFileV2& outFile
		);

		[[nodiscard]] static bool ReadInitChunk(
			BinaryReader& reader, DemoFileV2& outFile
		);

		[[nodiscard]] static bool ReadCommandsChunk(
			BinaryReader& reader, DemoFileV2& outFile
		);

		[[nodiscard]] static bool ReadSnapChunk(
			BinaryReader& reader, DemoFileV2& outFile
		);

		static void SortLoadedData(DemoFileV2& file);
	};
}
