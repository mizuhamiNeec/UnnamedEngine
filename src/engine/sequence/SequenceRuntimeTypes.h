#pragma once

#include <cstdint>

namespace Unnamed {
	/// @brief シーケンスプレイヤーの再生状態です。
	enum class SEQUENCE_PLAYER_STATE : uint8_t {
		STOPPED   = 0,
		PLAYING   = 1,
		PAUSED    = 2,
		COMPLETED = 3,
	};

	/// @brief 再生完了時の状態保持モードです。
	enum class SEQUENCE_COMPLETION_MODE : uint8_t {
		RESTORE_STATE = 0,
		KEEP_STATE    = 1,
	};

	/// @brief シーケンス再生方向です。
	enum class SEQUENCE_PLAYBACK_DIRECTION : uint8_t {
		FORWARD  = 0,
		BACKWARD = 1,
	};

	/// @brief シーク時のイベント評価方針です。
	enum class SEQUENCE_SEEK_EVENT_POLICY : uint8_t {
		SUPPRESS      = 0,
		FIRE_IN_RANGE = 1,
	};

	/// @brief 1ティック内でのシーケンス走査区間です。
	struct SequenceTraversalRange final {
		float                       startFrame = 0.0f;
		float                       endFrame   = 0.0f;
		SEQUENCE_PLAYBACK_DIRECTION direction  =
			SEQUENCE_PLAYBACK_DIRECTION::FORWARD;
		bool     wrapped         = false;
		int32_t  loopCount       = 0;
		bool     causedBySeek    = false;
		uint64_t traversalSerial = 0;
		bool     valid           = false;
	};
}
