#pragma once
#ifdef _DEBUG

#include <cstdint>

namespace Unnamed {
	/// @brief Sequence Editorで選択中のFloatチャンネル種別です。
	enum class SEQUENCE_EDITOR_FLOAT_CHANNEL : uint8_t {
		NONE = 0,
		FLOAT,
		VEC3_X,
		VEC3_Y,
		VEC3_Z,
		TRANSFORM_POS_X,
		TRANSFORM_POS_Y,
		TRANSFORM_POS_Z,
		TRANSFORM_ROT_X,
		TRANSFORM_ROT_Y,
		TRANSFORM_ROT_Z,
		TRANSFORM_ROT_W,
		TRANSFORM_SCALE_X,
		TRANSFORM_SCALE_Y,
		TRANSFORM_SCALE_Z,
		SKELETAL_WEIGHT,
		SKELETAL_PLAYBACK,
		SKELETAL_SPEED,
	};

	/// @brief Sequence Editorの選択状態です。
	struct SequenceEditorSelection final {
		int32_t                       trackIndex   = -1;
		int32_t                       sectionIndex = -1;
		SEQUENCE_EDITOR_FLOAT_CHANNEL floatChannel =
			SEQUENCE_EDITOR_FLOAT_CHANNEL::NONE;
		int32_t keyIndex = -1;
	};
}

#endif
