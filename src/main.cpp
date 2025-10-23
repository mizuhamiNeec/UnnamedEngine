#include <pch.h>

#include <engine/Engine.h>
#include <engine/platform/Win32App.h>
#include <engine/uengine/UEngine.h>

/// @brief エントリーポイント
/// @param hInstance インスタンスハンドル
/// @param hPrevInstance 以前のインスタンスハンドル（未使用）
/// @param lpCmdLine コマンドライン引数
/// @param nShowCmd ウィンドウ表示オプション（未使用）
/// @return 終了コード
int WINAPI wWinMain(
	[[maybe_unused]] const HINSTANCE hInstance,
	[[maybe_unused]] HINSTANCE       hPrevInstance,
	[[maybe_unused]] const PWSTR     lpCmdLine,
	[[maybe_unused]] const int       nShowCmd
) {
	// リークチェック
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	[[maybe_unused]] HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	// デフォルトは旧エンジン. 引数があったら新しい方を起動する
	const bool startNewEngine =
		lpCmdLine != nullptr && std::wcsstr(lpCmdLine, L"-new") != nullptr;

	if (startNewEngine) {
		const auto uEngine = std::make_unique<Unnamed::UEngine>();
		uEngine->Run();
	} else {
		const auto engine = std::make_unique<Unnamed::Engine>();

		if (!engine->Init()) {
			UASSERT(false && "Failed to initialize Engine");
		}
		while (!Win32App::PollEvents()) {
			if (OldWindowManager::ProcessMessage()) {
				break;
			}
			engine->Update();
		}

		engine->Shutdown();
	}

	CoUninitialize();
	return EXIT_SUCCESS;
}
