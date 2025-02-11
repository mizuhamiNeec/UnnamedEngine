#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

class JobSystem;

class BaseJobTask : public std::enable_shared_from_this<BaseJobTask> {
public:
	virtual ~BaseJobTask();
	virtual void Execute() = 0;
	void ExecuteWrapper();
	void NotifyDependents();
	void AddDependent(const std::shared_ptr<BaseJobTask>& dependent);
	void DependencyFinished();
	void SetDependencyCount(int count);
	void SetJobSystem(JobSystem* jobSystem);
	void SetPriority(int priority);

protected:
	std::atomic<int> mRemainingDependencies_{ 0 }; // 未解決の依存件数
	int mPriority_ = 0; // 優先度(高いほど優先される)
	JobSystem* mJobSystem_ = nullptr; // タスクスケジュール先のJobSystem

private:
	std::mutex mDependentsMutex_;
	std::vector<std::weak_ptr<BaseJobTask>> mDependents_; // 依存しているタスク
};
