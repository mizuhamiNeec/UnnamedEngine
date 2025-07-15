#include "Audio.h"

#include <cassert>
#include <comdef.h>
#include <format>
#include <fstream>

#include <SubSystem/Console/Console.h>
#include <Lib/Utils/StrUtil.h>

Audio::Audio() {
	audioBuffer_ = {};
}

Audio::~Audio() {
	if (sourceVoice_) {
		sourceVoice_->Stop();
		sourceVoice_->DestroyVoice();
		sourceVoice_ = nullptr;
	}
}

bool Audio::LoadFromFile(IXAudio2* xAudio2, const char* filename) {
	SoundData soundData;
	if (!LoadWavFile(filename, soundData)) {
		return false;
	}

	// ソースボイスの作成前にエラーチェック
	if (!xAudio2) {
		Console::Print("[Audio] XAudio2インスタンスが無効です\n", kConTextColorError, Channel::ResourceSystem);
		return false;
	}

	// WAVEFORMATEX のチェック
	if (soundData.wfex.wFormatTag != WAVE_FORMAT_PCM) {
		Console::Print("[Audio] 未対応の音声フォーマットです\n", kConTextColorError, Channel::ResourceSystem);
		return false;
	}

	// ソースボイスの生成（詳細なエラーハンドリング）
	HRESULT hr = xAudio2->CreateSourceVoice(&sourceVoice_, &soundData.wfex);
	if (FAILED(hr)) {
		switch (hr) {
		case XAUDIO2_E_INVALID_CALL:
			Console::Print("[Audio] 無効な関数呼び出しです\n", kConTextColorError, Channel::ResourceSystem);
			break;
		case XAUDIO2_E_XMA_DECODER_ERROR:
			Console::Print("[Audio] XMAデコーダーエラーです\n", kConTextColorError, Channel::ResourceSystem);
			break;
		default:
			Console::Print(std::format("[Audio] ソースボイスの作成に失敗しました: {:#x}\n", hr),
				kConTextColorError, Channel::ResourceSystem);
		}
		return false;
	}

	// バッファの設定
	audioBuffer_.pAudioData = soundData.pBuffer;
	audioBuffer_.AudioBytes = soundData.bufferSize;
	audioBuffer_.Flags = XAUDIO2_END_OF_STREAM;

	// データを保持
	audioData_ = std::move(soundData);

	return true;
}

void Audio::Play(const bool isLoop) {
	if (!sourceVoice_) {
		return;
	}

	Stop();
	audioBuffer_.LoopCount = isLoop ? XAUDIO2_LOOP_INFINITE : 0;
	sourceVoice_->SubmitSourceBuffer(&audioBuffer_);
	sourceVoice_->Start();
	isPlaying_ = true;
}

void Audio::Stop() {
	if (!sourceVoice_) {
		return;
	}
	sourceVoice_->Stop();
	sourceVoice_->FlushSourceBuffers();
	isPlaying_ = false;
}

void Audio::Pause() {
	if (!sourceVoice_ || !isPlaying_) {
		return;
	}
	sourceVoice_->Stop();
	isPlaying_ = false;
}

void Audio::Resume() {
	if (!sourceVoice_ || isPlaying_) {
		return;
	}
	sourceVoice_->Start();
	isPlaying_ = true;
}

void Audio::SetVolume(float volume) const {
	if (!sourceVoice_) {
		return;
	}
	// 0.0f から 1.0f の範囲にクランプ
	volume = std::clamp(volume, 0.0f, 1.0f);
	sourceVoice_->SetVolume(volume);
}

void Audio::SetPitch(float pitch) const {
	if (!sourceVoice_) {
		return;
	}
	sourceVoice_->SetFrequencyRatio(pitch);
}

bool Audio::LoadWavFile(const std::string& filename, SoundData& outData) {
	// ファイルを開く
	std::ifstream file;
	file.open(filename, std::ios_base::binary);
	if (!file.is_open()) {
		Console::Print(std::format("[Audio] ファイルのオープンに失敗しました: {}\n", filename),
			kConTextColorError, Channel::ResourceSystem);
		return false;
	}

	// RIFFヘッダーの読み込みと検証
	RiffHeader riff;
	file.read(reinterpret_cast<char*>(&riff), sizeof(riff));
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0 || strncmp(riff.type, "WAVE", 4) != 0) {
		Console::Print("[Audio] 無効なWAVEファイル形式です\n",
			kConTextColorError, Channel::ResourceSystem);
		return false;
	}

	FormatChunk format = {};
	ChunkHeader chunkHeader;
	bool foundFmt = false;
	bool foundData = false;

	// チャンクの解析
	while (file.read(reinterpret_cast<char*>(&chunkHeader), sizeof(ChunkHeader))) {
		if (strncmp(chunkHeader.id, "fmt ", 4) == 0) {
			format.chunk = chunkHeader;
			if (format.chunk.size <= sizeof(WAVEFORMATEX)) {
				file.read(reinterpret_cast<char*>(&format.fmt), format.chunk.size);
			} else if (format.chunk.size <= sizeof(WAVEFORMATEXTENSIBLE)) {
				WAVEFORMATEXTENSIBLE wfext;
				file.read(reinterpret_cast<char*>(&wfext), format.chunk.size);
				format.fmt = wfext.Format;
			} else {
				Console::Print("[Audio] 未対応のフォーマットチャンクサイズです\n",
					kConTextColorError, Channel::ResourceSystem);
				return false;
			}
			foundFmt = true;
		} else if (strncmp(chunkHeader.id, "data", 4) == 0) {
			foundData = true;
			break;
		} else {
			file.seekg(chunkHeader.size, std::ios_base::cur);
		}
	}

	if (!foundFmt || !foundData) {
		Console::Print("[Audio] 必要なチャンクが見つかりません\n",
			kConTextColorError, Channel::ResourceSystem);
		return false;
	}

	// 音声データの読み込み
	char* pBuffer = new char[chunkHeader.size];
	file.read(pBuffer, chunkHeader.size);

	// フォーマット情報のログ出力
	Console::Print(std::format(
		"[Audio] 読み込み完了:\n  フォーマット: {}\n  チャンネル数: {}\n  サンプリングレート: {} Hz\n  ビット深度: {}\n",
		format.fmt.wFormatTag,
		format.fmt.nChannels,
		format.fmt.nSamplesPerSec,
		format.fmt.wBitsPerSample),
		kConTextColorCompleted, Channel::ResourceSystem);

	file.close();

	// 出力データの設定
	outData.wfex = format.fmt;
	outData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	outData.bufferSize = chunkHeader.size;

	return true;
}
