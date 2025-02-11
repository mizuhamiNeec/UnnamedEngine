#include "JobSystem.h"

#include <cassert>
#include <Windows.h>

//-----------------------------------------------------------------------------
// Purpose: コンストラクタ
// - name (std::string): スレッド名
// - threadCount (size_t): スレッド数
//-----------------------------------------------------------------------------
JobSystem::JobSystem(std::string name, const size_t threadCount)
	: mStop_(false),
	name_(std::move(name)) {
	for (size_t i = 0; i < threadCount; ++i) {
		mThreads_.emplace_back(
			[this, name, i] {
				std::string threadName = name + std::to_string(i);
				const HRESULT hr = SetThreadDescription(GetCurrentThread(),
					std::wstring(threadName.begin(), threadName.end()).c_str());
				assert(SUCCEEDED(hr));

				while (true) {
					std::function<void()> job;
					{
						std::unique_lock<std::mutex> lock(mQueueMutex_);
						mQueueCondition_.wait(lock, [this] {
							return mStop_ || !mJobQueue_.empty();
							});
						if (mStop_ && mJobQueue_.empty()) {
							return;
						}
						job = mJobQueue_.top().job;
						mJobQueue_.pop();
					}
					job();
				}
			}
		);
	}
}

//-----------------------------------------------------------------------------
// Purpose: デストラクタ
//-----------------------------------------------------------------------------
JobSystem::~JobSystem() {
	{
		std::lock_guard lock(mQueueMutex_);
		mStop_ = true;
	}
	mQueueCondition_.notify_all();
	for (auto& thread : mThreads_) {
		if (thread.joinable()) {
			thread.join();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: ジョブをキューに追加します
// - priority (int): 優先度 (高いほど優先されます)
// - job (std::function<void()>): ジョブ
//-----------------------------------------------------------------------------
void JobSystem::EnqueueJob(const int priority, std::function<void()> job) {
	{
		std::lock_guard lock(mQueueMutex_);
		mJobQueue_.push(JobItem{ .priority = priority, .job = std::move(job) });
	}
	mQueueCondition_.notify_one();
}

//-----------------------------------------------------------------------------
// Purpose: メインスレッドジョブを送信します
// - job (std::function<void()>): ジョブ
//-----------------------------------------------------------------------------
void JobSystem::SubmitMainThreadJob(std::function<void()> job) const {
	if (mMainThreadDispatcher_) {
		mMainThreadDispatcher_->Enqueue(std::move(job));
	}
}

//-----------------------------------------------------------------------------
// Purpose: メインスレッドディスパッチャを設定します
// - dispatcher (MainThreadDispatcher*): メインスレッドディスパッチャ
//-----------------------------------------------------------------------------
void JobSystem::SetMainThreadDispatcher(MainThreadDispatcher* dispatcher) {
	mMainThreadDispatcher_ = dispatcher;
}