#include <Engine.h>
#include <Windows.h>
#include <Lib/Utils/StrUtils.h>
#include <SubSystem/Console/ConVarManager.h>
#include <SubSystem/Console/Console.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, const PWSTR lpCmdLine, [[maybe_unused]] const int nShowCmd) {
#ifdef _DEBUG
	ConVarManager::RegisterConVar<bool>("verbose", true, "Enable verbose logging");
#else
	ConVarManager::RegisterConVar<bool>("verbose", false, "Enable verbose logging");
#endif
	Console::Print("command line arguments:\n", kConTextColorGray, Channel::CommandLine);
	Console::Print(StrUtils::ToString(lpCmdLine) + "\n", kConTextColorGray, Channel::CommandLine);
	ConVarManager::RegisterConVar<std::string>("launchargs", StrUtils::ToString(lpCmdLine), "Command line arguments");
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	{
		auto engine = std::make_unique<Engine>();
		try {
			engine->Run();
		} catch (const std::exception& e) {
			Console::Print(e.what(), kConTextColorError, Channel::Engine);
			return EXIT_FAILURE;
		}
		D3DResourceLeakChecker leakChecker;
		engine.reset();
	}
	CoUninitialize();
	return EXIT_SUCCESS;
}
