#include <format>
#include <Windows.h>

#ifdef _DEBUG
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "imgui/imgui_internal.h"
#endif

#include "../Console.h"
#include "../ConVar.h"
#include "../Input.h"
#include "../Sprite.h"
#include "../SpriteManager.h"
#include "Engine/Renderer/D3D12.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Utils/ClientProperties.h"
#include "Engine/Utils/ConvertString.h"
#include "Game/GameScene.h"

// audio
#include <xaudio2.h>

#pragma comment(lib, "xaudio2.lib")
#include <fstream>

// チャンクヘッダ
struct ChunkHeader {
	char id[4]; // チャンク毎のID
	int32_t size; // チャンクサイズ
};

// RIFFヘッダチャンク
struct RiffHeader {
	ChunkHeader chunk; // "RIFF"
	char type[4]; // "WAVE"
};

// FMTチャンク
struct FormatChunk {
	ChunkHeader chunk; // "fmt"
	WAVEFORMATEX fmt; // 波形フォーマット
};

// 音声データ
struct SoundData {
	WAVEFORMATEX wfex; // 波形フォーマット
	BYTE* pBuffer; // バッファの先頭サイズ
	unsigned int bufferSize; // バッファのサイズ
};

SoundData SoundLoadWave(const char* filename) {
	//-----------------------------------------------------------------------------
	// ファイルオープン
	//-----------------------------------------------------------------------------
	std::ifstream file;
	file.open(filename, std::ios_base::binary);
	if (!file.is_open()) {
		Console::Print(std::format("Failed to open file: {}", filename));
		assert(file.is_open());
	}

	//-----------------------------------------------------------------------------
	// .wavデータ読み込み
	//-----------------------------------------------------------------------------
	RiffHeader riff;
	file.read(reinterpret_cast<char*>(&riff), sizeof(riff));
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
		Console::Print("File is not RIFF format");
		assert(0);
	}
	if (strncmp(riff.type, "WAVE", 4) != 0) {
		Console::Print("File is not WAVE format");
		assert(0);
	}

	FormatChunk format = {};
	ChunkHeader chunkHeader;

	bool foundFmt = false;
	bool foundData = false;

	while (file.read(reinterpret_cast<char*>(&chunkHeader), sizeof(ChunkHeader))) {
		if (strncmp(chunkHeader.id, "fmt ", 4) == 0) {
			// チャンク本体の読み込み
			format.chunk = chunkHeader;
			if (format.chunk.size <= sizeof(WAVEFORMATEX)) {
				file.read(reinterpret_cast<char*>(&format.fmt), format.chunk.size);
			} else if (format.chunk.size <= sizeof(WAVEFORMATEXTENSIBLE)) {
				WAVEFORMATEXTENSIBLE wfext;
				file.read(reinterpret_cast<char*>(&wfext), format.chunk.size);
				format.fmt = wfext.Format; // WAVEFORMATEX部分だけコピー
			} else {
				Console::Print("Unsupported format chunk size");
				assert(0);
			}
			foundFmt = true;
		} else if (strncmp(chunkHeader.id, "data", 4) == 0) {
			// Dataチャンクの処理
			foundData = true;
			break; // dataチャンクが見つかったのでループを抜ける
		} else {
			// 他のチャンクの場合はスキップ
			file.seekg(chunkHeader.size, std::ios_base::cur);
		}
	}

	if (!foundFmt) {
		Console::Print("Format chunk ID is not 'fmt '");
		assert(0);
	}

	if (!foundData) {
		Console::Print("Data chunk not found");
		assert(0);
	}

	char* pBuffer = new char[chunkHeader.size];
	file.read(pBuffer, chunkHeader.size);

	file.close();

	SoundData soundData = {};
	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = chunkHeader.size;

	return soundData;
}

// 音声データ開放
void SoundUnload(SoundData* soundData) {
	delete[] soundData->pBuffer;
	soundData->pBuffer = nullptr;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData) {
	HRESULT result;

	// 波形フォーマットを先にSourceVoiceの生成
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	if (FAILED(result)) {
		Console::Print(std::format("CreateSourceVoice failed: HRESULT = {:#010x}", result));
		assert(SUCCEEDED(result));
	}

	// 再生する波形データの設定
	XAUDIO2_BUFFER buf = {};
	buf.pAudioData = soundData.pBuffer;
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;

	// 波形データの生成
	result = pSourceVoice->SubmitSourceBuffer(&buf);
	if (FAILED(result)) {
		Console::Print(std::format("SubmitSourceBuffer failed: HRESULT = {:#010x}", result));
		assert(SUCCEEDED(result));
	}

	result = pSourceVoice->Start();
	if (FAILED(result)) {
		Console::Print(std::format("Start failed: HRESULT = {:#010x}", result));
		assert(SUCCEEDED(result));
	}
}

//-----------------------------------------------------------------------------
// エントリーポイント
//-----------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR pCmdLine, const int nCmdShow) {
	Console::Print(ConvertString(std::format(L"Launch Args: {}\n", pCmdLine)));

	ConVar cl_showpos("cl_showpos", 1, "Draw current position at top of screen");
	ConVar cl_showfps("cl_showfps", 1, "Draw fps meter (1 = fps)");

	Console::SubmitCommand(ConvertString(pCmdLine));

	D3DResourceLeakChecker leakChecker;

	std::unique_ptr<Window> window = std::make_unique<Window>();
	WindowConfig windowConfig = {
		kWindowTitle,
		kClientWidth,
		kClientHeight,
		kWindowClassName,
		WS_OVERLAPPEDWINDOW,
		WS_EX_WINDOWEDGE,
		nCmdShow
	};

	window->CreateMainWindow(windowConfig);

	std::unique_ptr<D3D12> renderer = std::make_unique<D3D12>();
	renderer->Init(window.get());

	std::unique_ptr<SpriteManager> spriteManager = std::make_unique<SpriteManager>();
	spriteManager->Init(renderer.get());

	// ---------------------------------------------------------------------------
	// 入力
	// ---------------------------------------------------------------------------
	std::unique_ptr<Input> input = std::make_unique<Input>();
	input->Init(window.get());

#ifdef _DEBUG
	ImGuiManager imGuiManager;
	imGuiManager.Init(renderer.get(), window.get());

	Console console;
#endif

#pragma region シーンの初期化

	GameScene gameScene;
	gameScene.Init(renderer.get(), window.get());

	Sprite* sprite = new Sprite();
	sprite->Init();

#pragma endregion

	ConVar cl_showpos("cl_showpos", 0, "Draw current position at top of screen");
	ConVar cl_showfps("cl_showfps", 1, "Draw fps meter (1 = fps)");

	ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice;

	// XAudioエンジンのインスタンスを生成
	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);

	// マスターボイスを生成
	result = xAudio2->CreateMasteringVoice(&masterVoice);

	// 音声読み込み
	SoundData soundData1 = SoundLoadWave("./Resources/Sounds/ニビイロドロウレ_Ayase.wav");

	// 音声再生
	SoundPlayWave(xAudio2.Get(), soundData1);

	while (true) {
		if (window->ProcessMessage()) {
			break; // ゲームループを抜ける
		}

		input->Update();

		// コンソールの表示切り替え
		if (input->TriggerKey(DIK_GRAVE)) {
			Console::ToggleConsole();
		}

	#ifdef _DEBUG
		imGuiManager.NewFrame();
		console.Update();
#endif

		// ゲームシーンの更新
		gameScene.Update();
    
#ifdef _DEBUG
		if (ConVars::GetInstance().GetConVar("cl_showfps")->GetInt() == 1) {
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f,0.0f});

			ImGuiWindowFlags windowFlags =
				ImGuiWindowFlags_NoBackground |
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoSavedSettings;

			ImVec2 windowPos = ImVec2(0.0f, 128.0f);
			ImVec2 windowSize = ImVec2(1080.0f, 80.0f);

			windowPos.x = ImGui::GetMainViewport()->Pos.x + windowPos.x;
			windowPos.y = ImGui::GetMainViewport()->Pos.y + windowPos.y;

			ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
			ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

			ImGui::Begin("##cl_showfps", nullptr, windowFlags);

			ImVec2 textPos = ImGui::GetCursorScreenPos();

			ImDrawList* drawList = ImGui::GetWindowDrawList();

			float outlineSize = 1.0f;

			std::string text = std::format("{:.2f} fps", ImGui::GetIO().Framerate);

			ImU32 textColor = IM_COL32(255, 0, 0, 255);
			ImU32 outlineColor = IM_COL32(0, 0, 0, 94);

			TextOutlined(
				drawList,
				textPos,
				text.c_str(),
				textColor,
				outlineColor,
				outlineSize
			);

			ImGui::PopStyleVar();

			ImGui::End();
		}
#endif

		// レンダリングの前処理
		renderer->PreRender();

		// ゲームシーンのレンダリング
		gameScene.Render();

	#ifdef _DEBUG
		imGuiManager.EndFrame();
	#endif

		// レンダリングの後処理
		renderer->PostRender();
	}

	gameScene.Shutdown();

#ifdef _DEBUG
	imGuiManager.Shutdown();
#endif

	delete sprite;

	TextureManager::GetInstance().Shutdown();

	return 0;
}
