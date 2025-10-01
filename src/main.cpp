#include <pch.h>

#include <engine/uengine/UEngine.h>

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

	const auto uEngine = std::make_unique<Unnamed::UEngine>();
	uEngine->Run();

	// const auto engine = std::make_unique<Unnamed::Engine>();

	//if (!engine->Init()) { UASSERT(false && "Failed to initialize Engine"); }
	// while (!Win32App::PollEvents()) {
	// 	// auto context = device->BeginFrame();
	// 	//
	// 	// device->EndFrame(context);
	//
	// 	// if (OldWindowManager::ProcessMessage()) {
	// 	// 	break;
	// 	// }
	// 	// engine->Update();
	// }

	//	engine->Shutdown();
	CoUninitialize();
	return EXIT_SUCCESS;
}
