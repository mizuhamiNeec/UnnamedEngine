#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/math/Vec3.h"

namespace Unnamed {
	/// @brief シーケンスの補間方式です。
	enum class SEQUENCE_INTERPOLATION_MODE : uint8_t {
		MODE_STEP   = 0,
		MODE_LINEAR = 1,
		MODE_CUBIC  = 2,
	};

	/// @brief シーケンスのブレンド方式です。
	enum class SEQUENCE_BLEND_MODE : uint8_t {
		MODE_ABSOLUTE = 0,
		MODE_ADDITIVE = 1,
	};

	/// @brief シーケンストラックの種類です。
	enum class SEQUENCE_TRACK_TYPE : uint8_t {
		TRANSFORM        = 0,
		SKELETAL_CONTROL = 1,
		CAMERA_CUT       = 2,
		EVENT            = 3,
		VISIBILITY       = 4,
		ACTIVATION       = 5,
		PROPERTY_FLOAT   = 6,
		PROPERTY_BOOL    = 7,
		PROPERTY_VEC3    = 8,
	};

	/// @brief Floatキーフレームです。
	struct SequenceFloatKeyAssetData {
		uint64_t                    keyId         = 0;
		int64_t                     frame         = 0;
		float                       value         = 0.0f;
		float                       arriveTangent = 0.0f;
		float                       leaveTangent  = 0.0f;
		SEQUENCE_INTERPOLATION_MODE interpolation =
			SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR;
	};

	/// @brief Boolキーフレームです。
	struct SequenceBoolKeyAssetData {
		uint64_t keyId = 0;
		int64_t  frame = 0;
		bool     value = false;
	};

	/// @brief Vec3キーフレームです。
	struct SequenceVec3KeyAssetData {
		uint64_t keyId = 0;
		int64_t  frame = 0;
		Vec3     value = Vec3::zero;
	};

	/// @brief カメラカットキーフレームです。
	struct SequenceCameraCutKeyAssetData {
		uint64_t keyId            = 0;
		int64_t  frame            = 0;
		uint64_t cameraEntityGuid = 0;
	};

	/// @brief イベントキーフレームです。
	struct SequenceEventKeyAssetData {
		uint64_t    keyId            = 0;
		int64_t     frame            = 0;
		std::string cueId            = "";
		uint64_t    sourceEntityGuid = 0;
		float       cueValue         = 0.0f;
		float       cueValue2        = 0.0f;
		std::string consoleCommand   = "";
	};

	/// @brief Floatカーブチャンネルです。
	struct SequenceRichCurveAssetData {
		uint64_t                               channelId = 0;
		std::vector<SequenceFloatKeyAssetData> keys      = {};
	};

	/// @brief セクション内で扱うスケルタル制御データです。
	struct SequenceSkeletalControlData {
		std::string                stateId       = "";
		int32_t                    layerIndex    = 0;
		bool                       playOnEnter   = false;
		bool                       stopOnExit    = false;
		SequenceRichCurveAssetData weightCurve   = {};
		SequenceRichCurveAssetData playbackCurve = {};
		SequenceRichCurveAssetData speedCurve    = {};
	};

	/// @brief トラック上の時間範囲を表すセクションです。
	struct SequenceSectionAssetData {
		uint64_t sectionId  = 0;
		int64_t  startFrame = 0;
		int64_t  endFrame   = 0;
		int32_t  priority   = 0;

		SequenceRichCurveAssetData            floatCurve = {};
		std::vector<SequenceBoolKeyAssetData> boolKeys   = {};
		SequenceRichCurveAssetData            vec3XCurve = {};
		SequenceRichCurveAssetData            vec3YCurve = {};
		SequenceRichCurveAssetData            vec3ZCurve = {};

		SequenceRichCurveAssetData transformPosX   = {};
		SequenceRichCurveAssetData transformPosY   = {};
		SequenceRichCurveAssetData transformPosZ   = {};
		SequenceRichCurveAssetData transformRotX   = {};
		SequenceRichCurveAssetData transformRotY   = {};
		SequenceRichCurveAssetData transformRotZ   = {};
		SequenceRichCurveAssetData transformRotW   = {};
		SequenceRichCurveAssetData transformScaleX = {};
		SequenceRichCurveAssetData transformScaleY = {};
		SequenceRichCurveAssetData transformScaleZ = {};

		std::vector<SequenceCameraCutKeyAssetData> cameraCutKeys = {};
		std::vector<SequenceEventKeyAssetData>     eventKeys     = {};
		SequenceSkeletalControlData                skeletal      = {};
	};

	/// @brief シーケンス対象のバインディング情報です。
	struct SequenceBindingAssetData {
		uint64_t    bindingId           = 0;
		uint64_t    entityGuid          = 0;
		std::string componentStableName = "";
		std::string propertyPath        = "";
	};

	/// @brief 1本のトラック情報です。
	struct SequenceTrackAssetData {
		uint64_t trackId = 0;
		std::string name = "";
		SEQUENCE_TRACK_TYPE trackType = SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT;
		SEQUENCE_BLEND_MODE blendMode = SEQUENCE_BLEND_MODE::MODE_ABSOLUTE;
		int32_t priority = 0;
		uint64_t bindingId = 0;
		std::vector<SequenceSectionAssetData> sections = {};
	};

	/// @brief シーケンスアセットのルートデータです。
	struct SequenceAssetData {
		int32_t                               version        = 1;
		std::string                           name           = "";
		int32_t                               displayRate    = 60;
		int32_t                               tickResolution = 24000;
		int64_t                               lengthFrames   = 0;
		std::vector<SequenceBindingAssetData> bindings       = {};
		std::vector<SequenceTrackAssetData>   tracks         = {};
	};
}
