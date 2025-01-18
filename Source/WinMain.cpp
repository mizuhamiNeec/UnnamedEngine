#include <Engine.h>
#include <format>
#include <Windows.h>
#include <Lib/Console/Console.h>
#include <Lib/Console/ConVarManager.h>
#include <Lib/Utils/StrUtils.h>

//-----------------------------------------------------------------------------
// Purpose: エントリーポイント
//-----------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR lpCmdLine, [[maybe_unused]] const int nShowCmd) {
	// TODO: リリース時にはverboseをfalseにする
	ConVarManager::RegisterConVar<bool>("verbose", true, "Enable verbose logging");
	Console::Print("command line arguments:\n", kConsoleColorGray, Channel::CommandLine);
	Console::Print(StrUtils::ToString(lpCmdLine) + "\n", kConsoleColorGray, Channel::CommandLine);
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	D3DResourceLeakChecker leakChecker;
	const auto engine = std::make_unique<Engine>();

	engine->Run();

	CoUninitialize();

	return 0;
}
