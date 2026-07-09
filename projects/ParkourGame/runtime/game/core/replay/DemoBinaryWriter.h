#pragma once

#include <string>
#include <string_view>

#include <json.hpp>

#include "DemoTypes.h"

#include "core/io/binary/BinaryWriter.h"

namespace Unnamed {
	class DemoBinaryWriter {
	public:
		[[nodiscard]] bool WriteFile(
			const Path&       path,
			const DemoFileV2& file,
			std::string*      outError = nullptr
		) const;

	private:
		static void SetError(
			std::string* outError, std::string_view text
		);

		template <typename Fn>
		[[nodiscard]] static bool WriteChunk(
			BinaryWriter&    writer,
			uint32_t         chunkId,
			Fn&&             payloadWriter,
			std::string*     outError,
			std::string_view chunkName
		);

		[[nodiscard]] static uint32_t EntityTypeId(std::string_view text);

		[[nodiscard]] static bool WriteHeadChunk(
			BinaryWriter&     writer,
			const DemoFileV2& file,
			std::string*      outError
		);

		[[nodiscard]] static bool WriteInitChunk(
			BinaryWriter&     writer,
			const DemoFileV2& file,
			std::string*      outError
		);

		[[nodiscard]] static bool WriteCommandsChunk(
			BinaryWriter&     writer,
			const DemoFileV2& file,
			std::string*      outError
		);

		[[nodiscard]] static bool WriteSnapChunk(
			BinaryWriter&     writer,
			const DemoFileV2& file,
			std::string*      outError
		);
	};
}
