#include <format>
#include <Windows.h>

#ifdef _DEBUG
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include "Engine/ImGuiManager/ImGuiManager.h"
#endif

#include "Engine/Renderer/D3D12.h"
#include "Engine/TextureManager/TextureManager.h"

// audio TODO : さっさとクラス化しよう
#include <xaudio2.h>

#pragma comment(lib, "xaudio2.lib")
#include <fstream>
#include "../Engine.h"

#include "Engine/Lib/Console/Console.h"
#include "Engine/Lib/Console/ConVar.h"
#include "Engine/Lib/Utils/ConvertString.h"

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
	// 波形フォーマットを先にSourceVoiceの生成
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	HRESULT result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	if (FAILED(result)) {
		Console::Print(std::format("CreateSourceVoice failed: HRESULT = {:#010x}\n", result));
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
		Console::Print(std::format("SubmitSourceBuffer failed: HRESULT = {:#010x}\n", result));
		assert(SUCCEEDED(result));
	}

	result = pSourceVoice->Start();
	if (FAILED(result)) {
		Console::Print(std::format("Start failed: HRESULT = {:#010x}\n", result));
		assert(SUCCEEDED(result));
	}
}

//-----------------------------------------------------------------------------
// エントリーポイント
//-----------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR pCmdLine, const int nShowCmd) {
	Console::Print(ConvertString::ToString(std::format(L"Launch Args: {}\n", pCmdLine)));

	ConVar cl_showpos("cl_showpos", 1, "Draw current position at top of screen");
	ConVar cl_showfps("cl_showfps", 1, "Draw fps meter (1 = fps)");
	ConVar cl_nshowcmd("cl_nshowcmd", nShowCmd, "");
	ConVar cl_maxfps("cl_maxfps", 60.0f, "max frame rate.");

	D3DResourceLeakChecker leakChecker;

	std::unique_ptr<Engine> engine = std::make_unique<Engine>();
	engine->Run();

	// 音声
	{
		ComPtr<IXAudio2> xAudio2;
		IXAudio2MasteringVoice* masterVoice;

		// TODO : コンソール変数を編集できるように

		// XAudioエンジンのインスタンスを生成
		HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
		if (FAILED(result)) {
			assert(0 && "なんかだめだったみたい");
		}

		// マスターボイスを生成
		result = xAudio2->CreateMasteringVoice(&masterVoice);

		// 音声読み込み
		SoundData soundData1 = SoundLoadWave("./Resources/Sounds/mokugyo.wav");

		// 音声再生
		SoundPlayWave(xAudio2.Get(), soundData1);
	}

	return 0;
}
