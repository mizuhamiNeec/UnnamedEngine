#include <pch.h>

#include <chrono>
#include <filesystem>

#include <core/filesystem/Path.h>
#include <engine/unnamed/subsystem/console/ConsoleFileLogSink.h>
#include <engine/unnamed/subsystem/time/SystemClock.h>

namespace Unnamed {
	static constexpr std::string_view kChannel = "ConFLS";

	ConsoleFileLogSink::ConsoleFileLogSink() = default;

	ConsoleFileLogSink::~ConsoleFileLogSink() {
		Stop();
	}

	bool ConsoleFileLogSink::Start(const Config& cfg) {
		Stop();

		mCfg = cfg;

		// 既存ファイルを削除
		if (!mCfg.path.IsEmpty() &&
		    std::filesystem::exists(mCfg.path.Native())) {
			std::error_code ec;
			std::filesystem::remove(mCfg.path.Native(), ec);
		}

		if (!OpenFile()) {
			return false;
		}

		mRunning.store(true, std::memory_order_release);
		mThread = std::thread(&ConsoleFileLogSink::ThreadMain, this);

		Msg(
			kChannel, "Console file log sink started. Logging to '{}'",
			mCfg.path
		);

		return true;
	}

	void ConsoleFileLogSink::Stop() {
		const bool wasRunning = mRunning.exchange(false);
		if (!wasRunning) {
			return;
		}

		mCv.notify_all();
		if (mThread.joinable()) {
			mThread.join();
		}

		// 念のため残りを同期で書き出し
		std::vector<Event> remaining;
		{
			std::scoped_lock lock(mMutex);
			remaining.swap(mQueue);
		}
		WriteBatch(remaining);

		if (mFile.is_open()) {
			mFile.flush();
			mFile.close();
		}
	}

	void ConsoleFileLogSink::Enqueue(Event&& e) {
		if (!mRunning.load(std::memory_order_acquire)) {
			return;
		}

		e.sequence = mSequence.fetch_add(1, std::memory_order_relaxed) + 1;
		{
			std::scoped_lock lock(mMutex);
			mQueue.emplace_back(std::move(e));
		}
		mCv.notify_one();
	}

	void ConsoleFileLogSink::FlushNow(const uint32_t timeoutMs) {
		if (!mRunning.load(std::memory_order_acquire)) {
			return;
		}

		uint64_t targetSeq;
		{
			std::scoped_lock lock(mMutex);
			targetSeq = mQueue.empty() ?
				            mSequence.load(std::memory_order_relaxed) :
				            mQueue.back().sequence;
		}

		mCv.notify_one();

		std::unique_lock lk(mFlushMutex);
		mFlushCv.wait_for(
			lk,
			std::chrono::milliseconds(timeoutMs),
			[&] {
				return mWrittenSequence.load(std::memory_order_acquire) >=
				       targetSeq;
			}
		);
	}

	void ConsoleFileLogSink::ThreadMain() {
		std::vector<Event> batch;
		batch.reserve(mCfg.batchSize);

		auto lastFlush = std::chrono::steady_clock::now();

		while (mRunning.load(std::memory_order_acquire)) {
			{
				std::unique_lock lk(mMutex);
				mCv.wait_for(
					lk,
					std::chrono::milliseconds(mCfg.flushIntervalMs),
					[&] {
						return !mRunning.load(std::memory_order_acquire) || !
						       mQueue.empty();
					}
				);

				batch.swap(mQueue);
			}

			if (!batch.empty()) {
				WriteBatch(batch);
				batch.clear();
			}

			auto now = std::chrono::steady_clock::now();
			if (mFile.is_open() && now - lastFlush >= std::chrono::milliseconds(
				    mCfg.flushIntervalMs
			    )) {
				mFile.flush();
				lastFlush = now;
			}
		}
	}

	void ConsoleFileLogSink::WriteBatch(std::vector<Event>& batch) {
		if (batch.empty() || !mFile.is_open()) {
			return;
		}

		static std::atomic channelWidth = {0};

		for (const auto& e : batch) {
			mFile << std::format(
				"{:02}/{:02} {:02}:{:02}:{:02} [{:^7}] ",
				e.timeStamp.month,
				e.timeStamp.day,
				e.timeStamp.hour,
				e.timeStamp.minute,
				e.timeStamp.second,
				ToString(e.level)
			);

			// チャンネル幅を「これまでの最大」に更新して中央揃えで出力
			const int w = e.channel.empty() ?
				              0 :
				              static_cast<int>(e.channel.size());
			int cur = channelWidth.load(std::memory_order_relaxed);
			while (w > cur && !channelWidth.compare_exchange_weak(
				       cur, w, std::memory_order_relaxed
			       )) {
			}

			const int outW = channelWidth.load(std::memory_order_relaxed);
			if (outW > 0) {
				mFile << std::format("[{:^{}}] ", e.channel, outW);
			}

			mFile << e.message;

			if (e.location.line() != 0) {
				mFile << std::format(
					" ({0}:{1})",
					e.location.file_name(),
					e.location.line()
				);
			}

			mFile << std::format(" (tid:{}) ", e.threadId);

			mFile << "\n";
			mWrittenSequence.store(e.sequence, std::memory_order_release);
		}

		mFlushCv.notify_all();
	}

	bool ConsoleFileLogSink::OpenFile() {
		mFile.open(
			mCfg.path.Native(),
			std::ios::out | std::ios::binary
		);
		if (!mFile.is_open()) {
			return false;
		}

		// ヘッダーを書き込み
		const auto now = SystemClock::GetDateTime(SystemClock::StartTime());
		const std::string header = std::format(
			"//-----------------------------------------------------------------------------\n"
			"// BuildDate: {}-{}\n"
			"// Engine: {} Ver. {}\n"
			"// LaunchDate: {:02}-{:02}-{:02} {:02}:{:02}:{:02}\n"
			"//-----------------------------------------------------------------------------\n\n",
			__DATE__, __TIME__,
			ENGINE_NAME, ENGINE_VERSION,
			now.year, now.month, now.day, now.hour, now.minute, now.second
		);
		mFile.write(
			header.c_str(), static_cast<std::streamsize>(header.size())
		);
		mFile.flush();
		return true;
	}
}
