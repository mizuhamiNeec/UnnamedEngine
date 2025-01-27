#include <Engine.h>
#include <format>
#define NOMINMAX
#include <Windows.h>
#include <SubSystem/Console/Console.h>
#include <SubSystem/Console/ConVarManager.h>
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
	{
		auto engine = std::make_unique<Engine>();
		engine->Run();
		D3DResourceLeakChecker leakChecker;
		engine.reset();
	}
	CoUninitialize();
	return 0;
}
