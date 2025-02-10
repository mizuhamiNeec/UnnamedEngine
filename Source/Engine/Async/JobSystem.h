#pragma once
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <Async/MainThreadDispatcher.h>

#include "JobTask/JobTask.h"
#include "JobTask\Base\BaseJobTask.h"

struct JobItem {
	int priority;
	std::function<void()> job;

	bool operator<(const JobItem& other) const {
		return priority < other.priority;
	}
};

class JobSystem {
public:
	explicit JobSystem(std::string name,size_t threadCount);
	~JobSystem();

	void EnqueueJob(int priority,std::function<void()> job);

	template<typename Func,typename... Args>
	auto SubmitJob(int priority,Func&& func,Args&&... args)
		-> std::future<typename std::invoke_result<Func,Args...>::type>
	{
		using ReturnType = typename std::invoke_result<Func,Args...>::type;
		auto taskPtr = std::make_shared<std::packaged_task<ReturnType()>>(
			std::bind(std::forward<Func>(func),std::forward<Args>(args)...)
		);
		auto future = taskPtr->get_future();

		// ラムダ内で taskPtr を通して実行する（shared_ptr はコピー可能）
		EnqueueJob(priority,[taskPtr]() {
			(*taskPtr)();
		});
		return future;
	}

	template<typename Func,typename... Args>
	auto SubmitDependentJob(const std::vector<std::shared_ptr<BaseJobTask>>& dependencies,int priority,Func&& func,Args&&... args)
		-> std::future<typename std::invoke_result<Func,Args...>::type>
	{
		using ReturnType = typename std::invoke_result<Func,Args...>::type;

		// パッケージタスクを生成
		std::packaged_task<ReturnType()> task(std::bind(std::forward<Func>(func),std::forward<Args>(args)...));

		// 新規タスクを作成
		auto jobTask = std::make_shared<JobTask<ReturnType>>(std::move(task));
		jobTask->SetJobSystem(this);
		jobTask->SetPriority(priority);
		jobTask->SetDependencyCount(static_cast<int>(dependencies.size()));

		// 各依存先に、自分を依存タスクとして登録
		for(auto& dep : dependencies) {
			dep->AddDependent(jobTask);
		}

		// 依存がない場合はすぐにキューに投入
		if(dependencies.empty()) {
			EnqueueJob(priority,[jobTask]() {
				jobTask->ExecuteWrapper();
			});
		}

		return jobTask->GetFuture();
	}

	void SubmitMainThreadJob(std::function<void()> job) const;
	void SetMainThreadDispatcher(MainThreadDispatcher* dispatcher);

private:
	std::vector<std::thread> mThreads;
	std::priority_queue<JobItem> mJobQueue;
	std::mutex mQueueMutex;
	std::condition_variable mQueueCondition;
	bool mStop;

	MainThreadDispatcher* mMainThreadDispatcher = nullptr;
};