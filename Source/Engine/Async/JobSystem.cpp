#include "JobSystem.h"

#include <cassert>
#include <Windows.h>

JobSystem::JobSystem(std::string name,const size_t threadCount)
	: mStop(false)
{
	for(size_t i = 0; i < threadCount; ++i) {
		mThreads.emplace_back(
			[this,name,i] {
			std::string threadName = name + std::to_string(i);
			HRESULT hr = SetThreadDescription(GetCurrentThread(),
				std::wstring(threadName.begin(),threadName.end()).c_str());
			assert(SUCCEEDED(hr));

			while(true) {
				std::function<void()> job;
				{
					std::unique_lock<std::mutex> lock(mQueueMutex);
					mQueueCondition.wait(lock,[this] {
						return mStop || !mJobQueue.empty();
					});
					if(mStop && mJobQueue.empty()) {
						return;
					}
					job = mJobQueue.top().job;
					mJobQueue.pop();
				}
				job();
			}
		}
		);
	}
}

JobSystem::~JobSystem() {
	{
		std::lock_guard<std::mutex> lock(mQueueMutex);
		mStop = true;
	}
	mQueueCondition.notify_all();
	for(auto& thread : mThreads) {
		if(thread.joinable()) {
			thread.join();
		}
	}
}

void JobSystem::EnqueueJob(int priority,std::function<void()> job) {
	{
		std::lock_guard<std::mutex> lock(mQueueMutex);
		mJobQueue.push(JobItem{priority,std::move(job)});
	}
	mQueueCondition.notify_one();
}

void JobSystem::SubmitMainThreadJob(std::function<void()> job) const {
	if(mMainThreadDispatcher) {
		mMainThreadDispatcher->Enqueue(std::move(job));
	}
}

void JobSystem::SetMainThreadDispatcher(MainThreadDispatcher* dispatcher) {
	mMainThreadDispatcher = dispatcher;
}