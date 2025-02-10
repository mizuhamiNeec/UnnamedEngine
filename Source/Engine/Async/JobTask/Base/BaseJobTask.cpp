#include "BaseJobTask.h"

#include <Async/JobSystem.h>

BaseJobTask::~BaseJobTask() {}

void BaseJobTask::ExecuteWrapper() {
	Execute();
	NotifyDependents();
}

void BaseJobTask::NotifyDependents() {
	std::lock_guard lock(mDependentsMutex);
	for(auto& weakDep : mDependents) {
		if(const auto dep = weakDep.lock()) {
			dep->DependencyFinished();
		}
	}
}

void BaseJobTask::AddDependent(const std::shared_ptr<BaseJobTask>& dependent) {
	std::lock_guard lock(mDependentsMutex);
	mDependents.push_back(dependent);
}

void BaseJobTask::DependencyFinished() {
	int prev = mRemainingDependencies.fetch_sub(1,std::memory_order_relaxed);
	if(prev == 1) {
		// すべての依存関係が完了したのでジョブシステム経由でキューに登録
		if(mJobSystem) {
			mJobSystem->EnqueueJob(mPriority,[self = shared_from_this()]() {
				self->ExecuteWrapper();
			});
		}
	}
}

void BaseJobTask::SetDependencyCount(const int count) {
	mRemainingDependencies.store(count,std::memory_order_relaxed);
}

void BaseJobTask::SetJobSystem(JobSystem* jobSystem) {
	mJobSystem = jobSystem;
}

void BaseJobTask::SetPriority(const int priority) {
	mPriority = priority;
}
