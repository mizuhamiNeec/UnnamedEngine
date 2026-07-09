#pragma once

#include <cstdint>
#include <string>

namespace Unnamed {
	class Path;
	class Entity;
	struct DemoTickCommand;

	/// @brief デモ録画・再生サービスの抽象インターフェースです。
	/// @details Engine はこの抽象のみを参照し、ゲーム固有の実装を直接参照しません。
	class IDemoService {
	public:
		virtual ~IDemoService() = default;

		/// @brief デモ録画を開始します。
		virtual bool StartRecording(Path path) = 0;
		/// @brief デモ再生を開始します。
		virtual bool StartPlayback(Path path) = 0;
		/// @brief 録画または再生を停止します。
		virtual bool Stop() = 0;

		/// @brief 録画中かどうかを返します。
		[[nodiscard]] virtual bool IsRecording() const = 0;
		/// @brief 再生中かどうかを返します。
		[[nodiscard]] virtual bool IsPlayback() const = 0;
		/// @brief 再生が終了しているかどうかを返します。
		[[nodiscard]] virtual bool IsPlaybackFinished() const = 0;
		/// @brief 再生開始ティックを返します。
		[[nodiscard]] virtual uint64_t GetPlaybackStartTick() const = 0;
		/// @brief 現在扱っているデモパスを返します。
		[[nodiscard]] virtual Path GetCurrentPath() const = 0;
		/// @brief 再生セッション番号を返します。
		[[nodiscard]] virtual uint64_t GetPlaybackSessionSerial() const = 0;
		/// @brief 録画セッション番号を返します。
		[[nodiscard]] virtual uint64_t GetRecordingSessionSerial() const = 0;
		/// @brief シミュレーションtickrateを返します。
		[[nodiscard]] virtual uint32_t GetSimulationTickRate() const = 0;
		/// @brief シミュレーションの固定ステップ秒数を返します。
		[[nodiscard]] virtual float GetSimulationStepSeconds() const = 0;
		/// @brief 現在状態の文字列表現を返します。
		[[nodiscard]] virtual std::string BuildStatusString() const = 0;

		/// @brief ライブ入力コマンドを録画へ送信します。
		virtual void SubmitLiveCommand(const DemoTickCommand& command) = 0;
		/// @brief 再生コマンドを消費します。
		[[nodiscard]] virtual bool ConsumePlaybackCommand(
			uint64_t         tick,
			uint64_t         subjectEntityGuid,
			DemoTickCommand& outCommand
		) = 0;
		/// @brief 再生初期スナップショットのキャプチャを行います。
		virtual void CaptureInitialSnapshotIfNeeded(
			const Entity& subjectEntity
		) = 0;
		/// @brief 再生初期スナップショットの適用を行います。
		[[nodiscard]] virtual bool ApplyInitialSnapshotIfNeeded(
			Entity& subjectEntity
		) = 0;
		/// @brief 録画または再生のスナップショット検証を行います。
		virtual void RecordOrVerifySnapshot(
			uint64_t tick,
			Entity&  subjectEntity
		) = 0;

		/// @brief 設定されたtickrateを解決します。
		[[nodiscard]] static uint32_t ResolveConfiguredTickRate();
		/// @brief tickrate から固定ステップ秒数を計算します。
		[[nodiscard]] static float TickStepSecondsFromRate(uint32_t tickRate);
	};
}
