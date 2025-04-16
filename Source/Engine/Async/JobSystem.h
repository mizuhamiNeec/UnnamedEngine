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
#include <Async/JobTask/JobTask.h>
#include <Async/JobTask/Base/BaseJobTask.h>

struct JobItem {
	int priority;
	std::function<void()> job;

	bool operator<(const JobItem& other) const {
		return priority < other.priority;
	}
};

class JobSystem {
public:
	explicit JobSystem(std::string name, size_t threadCount);
	~JobSystem();

	void EnqueueJob(int priority, std::function<void()> job);

	template<typename Func, typename... Args>
	auto SubmitJob(int priority, Func&& func, Args&&... args)
		-> std::future<std::invoke_result_t<Func, Args...>>
		;

	template<typename Func, typename... Args>
	auto SubmitDependentJob(const std::vector<std::shared_ptr<BaseJobTask>>& dependencies, int priority, Func&& func, Args&&... args)
		-> std::future<std::invoke_result_t<Func, Args...>>
		;

	void SubmitMainThreadJob(std::function<void()> job) const;
	void SetMainThreadDispatcher(MainThreadDispatcher* dispatcher);

private:
	std::vector<std::thread> mThreads_;
	std::priority_queue<JobItem> mJobQueue_;
	std::mutex mQueueMutex_;
	std::condition_variable mQueueCondition_;
	bool mStop_;

	MainThreadDispatcher* mMainThreadDispatcher_ = nullptr;
	std::string name_;
};

template <typename Func, typename ... Args>
auto JobSystem::SubmitJob(
	int priority,
	Func&& func,
	Args&&... args
) -> std::future<std::invoke_result_t<Func, Args...>> {
	using ReturnType = std::invoke_result_t<Func, Args...>;
	auto taskPtr = std::make_shared<std::packaged_task<ReturnType()>>(
		std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
	);
	auto future = taskPtr->get_future();

	// ラムダ内で taskPtr を通して実行する（shared_ptr はコピー可能）
	EnqueueJob(priority, [taskPtr]() {
		(*taskPtr)();
		});
	return future;
}

template <typename Func, typename ... Args>
auto JobSystem::SubmitDependentJob(
	const std::vector<std::shared_ptr<BaseJobTask>>& dependencies,
	int priority,
	Func&& func,
	Args&&... args
) -> std::future<std::invoke_result_t<Func, Args...>> {
	using ReturnType = std::invoke_result_t<Func, Args...>;

	// パッケージタスクを生成
	std::packaged_task<ReturnType()> task(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

	// 新規タスクを作成
	auto jobTask = std::make_shared<JobTask<ReturnType>>(std::move(task));
	jobTask->SetJobSystem(this);
	jobTask->SetPriority(priority);
	jobTask->SetDependencyCount(static_cast<int>(dependencies.size()));

	// 各依存先に、自分を依存タスクとして登録
	for (auto& dep : dependencies) {
		dep->AddDependent(jobTask);
	}

	// 依存がない場合はすぐにキューに投入
	if (dependencies.empty()) {
		EnqueueJob(priority, [jobTask]() {
			jobTask->ExecuteWrapper();
			});
	}

	return jobTask->GetFuture();
}
