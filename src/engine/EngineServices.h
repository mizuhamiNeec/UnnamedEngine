#pragma once

namespace Unnamed {
	class ConsoleSystem;
	class InputSystem;
	class AssetManager;
	class Profiler;
	class WindowManager;
	class IDemoService;

	/// @brief ゲームモジュール初期化時に渡す Engine サービス群です。
	struct EngineServices {
		ConsoleSystem* console       = nullptr;
		InputSystem*   inputSystem   = nullptr;
		AssetManager*  assetManager  = nullptr;
		Profiler*      profiler      = nullptr;
		WindowManager* windowManager = nullptr;
		IDemoService*  demoService   = nullptr;
	};
}
