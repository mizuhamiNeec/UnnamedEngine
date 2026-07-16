#pragma once

namespace Unnamed {
	enum class EASE_TYPE {
		LINEAR,                                  // Linear
		IN_SINE, OUT_SINE, IN_OUT_SINE,          // Sine
		IN_QUAD, OUT_QUAD, IN_OUT_QUAD,          // Quad
		IN_CUBIC, OUT_CUBIC, IN_OUT_CUBIC,       // Cubic
		IN_QUART, OUT_QUART, IN_OUT_QUART,       // Quart
		IN_QUINT, OUT_QUINT, IN_OUT_QUINT,       // Quint
		IN_EXPO, OUT_EXPO, IN_OUT_EXPO,          // Expo
		IN_CIRC, OUT_CIRC, IN_OUT_CIRC,          // Circ
		IN_BACK, OUT_BACK, IN_OUT_BACK,          // Back
		IN_ELASTIC, OUT_ELASTIC, IN_OUT_ELASTIC, // Elastic
		IN_BOUNCE, OUT_BOUNCE, IN_OUT_BOUNCE,    // Bounce
	};

	enum class LOOP_TYPE {
		RESTART, // 最初から再生
		YOYO,    // 逆再生
	};

	enum class TWEEN_STATE {
		DELAYED,   // 遅延中
		PLAYING,   // 再生中
		PAUSED,    // 一時停止中
		COMPLETED, // 完了
		KILLED,    // 終了
	};
}
