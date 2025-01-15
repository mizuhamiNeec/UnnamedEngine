#include <Windows.h>
#include <format>

#include <fstream>

// audio TODO : さっさとクラス化しよう
#include <xaudio2.h>



#include <Engine.h>

#include "Engine/Lib/Console/Console.h"
#include "Engine/Lib/Utils/StrUtils.h"
#include "Lib/Console/ConVarManager.h"

//-----------------------------------------------------------------------------
// エントリーポイント
//-----------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR lpCmdLine, [[maybe_unused]] const int nShowCmd) {
	// TODO: リリース時にはverboseをfalseにする
	ConVarManager::RegisterConVar<bool>("verbose", true, "Enable verbose logging");
	Console::Print(
		StrUtils::ToString(std::format(L"command line arguments: {}\n", lpCmdLine)), kConsoleColorNormal,
		Channel::Console
	);
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	D3DResourceLeakChecker leakChecker;
	const auto engine = std::make_unique<Engine>();

	//// 音声
	//{
	//	ComPtr<IXAudio2> xAudio2;
	//	IXAudio2MasteringVoice* masterVoice;

	//	// TODO : コンソール変数を編集できるように

	//	// XAudioエンジンのインスタンスを生成
	//	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	//	if (FAILED(result)) {
	//		assert(0 && "なんかだめだったみたい");
	//	}

	//	// マスターボイスを生成
	//	result = xAudio2->CreateMasteringVoice(&masterVoice);
	//	if (FAILED(result)) {
	//		Console::Print(std::format("CreateMasteringVoice failed: HRESULT = {:#010x}\n", result));
	//		assert(SUCCEEDED(result));
	//	}
	//	// 音声読み込み
	//	SoundData soundData1 = SoundLoadWave("./Resources/Sounds/mokugyo.wav");

	//	// 音声再生
	//	SoundPlayWave(xAudio2.Get(), soundData1);
	//	Sleep(50);
	//}

	engine->Run();

	CoUninitialize();

	return 0;
}
