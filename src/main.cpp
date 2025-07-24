#include <pch.h>

#include <engine/public/Engine.h>
#include <engine/public/platform/Win32App.h>


int WINAPI wWinMain(
	[[maybe_unused]] const HINSTANCE hInstance,
	[[maybe_unused]] HINSTANCE       hPrevInstance,
	[[maybe_unused]] const PWSTR     lpCmdLine,
	[[maybe_unused]] const int       nShowCmd
) {
	_CrtSetDbgFlag( // リークチェック
		_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF
	);
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	//const auto app    = std::make_unique<Win32App>(hInstance);
	const auto engine = std::make_unique<Unnamed::Engine>();
	//if (!app->Init()) { UASSERT(false && "Failed to initialize Win32App"); }
	if (!engine->Init()) { UASSERT(false && "Failed to initialize Engine"); }
	while (true) {
		if (OldWindowManager::ProcessMessage()) {
			break;
		}
		engine->Update();
		// if (Win32App::PollEvents()) {
		// 	break;
		// }
	}
	engine->Shutdown();
	//app->Shutdown();
	CoUninitialize();
	return EXIT_SUCCESS;
}
