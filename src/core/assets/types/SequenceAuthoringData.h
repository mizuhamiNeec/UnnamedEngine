#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "SequenceAssetData.h"

namespace Unnamed {
	/// @brief Sequenceエディタ専用メタデータです。
	struct SequenceEditorMetadata final {
		bool scrubFireEvents = false;
		bool autoKeyEnabled  = false;
	};

	/// @brief Sequence編集用ルートデータです。
	struct SequenceAuthoringData final {
		int32_t                               version        = 2;
		std::string                           name           = "";
		int32_t                               displayRate    = 60;
		int32_t                               tickResolution = 24000;
		int64_t                               lengthFrames   = 0;
		std::vector<SequenceBindingAssetData> bindings       = {};
		std::vector<SequenceTrackAssetData>   tracks         = {};
		SequenceEditorMetadata                editor         = {};
	};
}
