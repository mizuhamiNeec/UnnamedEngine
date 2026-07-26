#pragma once
#include <string>

namespace Unnamed {
	enum class RUN_MODE {
		EDITOR,
		STANDALONE,
	};

	enum class WINDOW_MODE : uint8_t {
		WINDOWED,
		BORDERLESS,
		FULLSCREEN,
	};

	enum class BACKEND_TYPE : uint8_t {
		D3D12,
	};

	/// @brief EngineConfigは、Engine機能の生成時に適用する有効化条件と調整値を保持します
	struct EngineConfig {
		RUN_MODE mode;

		/// @brief Windowは、engine起動時のwindow title、client幅、高さ、表示設定を保持します
		struct Window {
			std::string title     = "Unnamed Engine";
			int32_t     width     = 1280;
			int32_t     height    = 720;
			WINDOW_MODE mode      = WINDOW_MODE::WINDOWED;
			bool        resizable = true;
		} window;
	};
}
