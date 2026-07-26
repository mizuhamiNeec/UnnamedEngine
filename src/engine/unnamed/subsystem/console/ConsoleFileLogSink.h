#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/filesystem/Path.h"
#include <engine/unnamed/subsystem/console/interface/IConsole.h>
#include <engine/unnamed/time/DateTime.h>

namespace Unnamed {
	/// @brief コンソールログをファイルへ非同期に書き込むためのシンク
	/// @details 別スレッドでキューに溜まったログイベントをファイルに書き出します。
	class ConsoleFileLogSink final {
	public:
		/// @brief Eventは、開発者コンソールで発生した事象と配送に必要な付随値を保持します
		struct Event {
			uint64_t             sequence = 0;
			LogLevel             level    = LogLevel::Info;
			std::string          channel;
			std::string          message;
			DateTime             timeStamp;
			std::source_location location;
			uint32_t             threadId = 0;
		};

		/// @brief Configは、機能の生成時に適用する有効化条件と調整値を保持します
		struct Config {
			Path     path            = Path("./consolesystem.log");
			uint32_t flushIntervalMs = 250;             // flushの間隔
			size_t   batchSize       = 128;             // この件数で即書き込み
			LogLevel flushOnLevel    = LogLevel::Error; // このレベル以上はすぐ書き込み
		};

		ConsoleFileLogSink();
		~ConsoleFileLogSink();

		// コピー禁止
		ConsoleFileLogSink(const ConsoleFileLogSink&)            = delete;
		ConsoleFileLogSink& operator=(const ConsoleFileLogSink&) = delete;

		/// @brief ログシンクを開始します
		/// @param cfg 設定
		/// @return 成功したらtrueを返す
		bool Start(const Config& cfg);

		/// @brief ログシンクを停止します（残りを書き込んでスレッド終了）
		/// @details 停止後は再度StartするまでEnqueueは無視されます
		void Stop();

		/// @brief イベントをキューに積みます
		void Enqueue(Event&& e);

		/// @brief できる限りキューにある分をファイルへ書き出します
		/// @details ヤベーときに呼んでください（Error/Fatal時など）
		void FlushNow(uint32_t timeoutMs = 50);

		/// @brief 設定取得
		[[nodiscard]] const Config& GetConfig() const {
			return mCfg;
		}

	private:
		/// @brief バックグラウンドスレッドのメインループ
		/// @details キューからイベントをバッチで取り出してファイルに書き込みます
		void ThreadMain();

		/// @brief イベントのバッチをファイルに書き込みます
		/// @param batch 書き込むイベントのバッチ
		void WriteBatch(std::vector<Event>& batch);

		/// @brief ログファイルを開きます
		/// @return 成功したらtrueを返す
		bool OpenFile();

		Config mCfg;

		std::atomic<bool> mRunning{false};
		std::thread       mThread;

		std::mutex              mMutex;
		std::condition_variable mCv;
		std::vector<Event>      mQueue;

		// FlushNow待ち合わせ用
		std::mutex              mFlushMutex;
		std::condition_variable mFlushCv;
		std::atomic<uint64_t>   mWrittenSequence{0};

		std::atomic<uint64_t> mSequence{0};
		std::ofstream         mFile;
	};
}
