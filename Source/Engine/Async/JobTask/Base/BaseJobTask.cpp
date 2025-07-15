#include "BaseJobTask.h"

#include <Async/JobSystem.h>

BaseJobTask::~BaseJobTask() = default;

//-----------------------------------------------------------------------------
// Purpose: タスクを実行します
//-----------------------------------------------------------------------------
void BaseJobTask::ExecuteWrapper() {
	Execute();
	NotifyDependents();
}

//-----------------------------------------------------------------------------
// Purpose: 依存関係を通知します
//-----------------------------------------------------------------------------
void BaseJobTask::NotifyDependents() {
	std::lock_guard lock(mDependentsMutex_);
	for (auto& weakDep : mDependents_) {
		if (const auto dep = weakDep.lock()) {
			dep->DependencyFinished();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 依存関係を追加します
//-----------------------------------------------------------------------------
void BaseJobTask::AddDependent(const std::shared_ptr<BaseJobTask>& dependent) {
	std::lock_guard lock(mDependentsMutex_);
	mDependents_.push_back(dependent);
}

//-----------------------------------------------------------------------------
// Purpose: 依存関係が完了したことを通知します
//-----------------------------------------------------------------------------
void BaseJobTask::DependencyFinished() {
	int prev = mRemainingDependencies_.fetch_sub(1, std::memory_order_relaxed);
	if (prev == 1) {
		// すべての依存関係が完了したのでジョブシステム経由でキューに登録
		if (mJobSystem_) {
			mJobSystem_->EnqueueJob(mPriority_, [self = shared_from_this()] {
				self->ExecuteWrapper();
				}
			);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 依存関係の数を設定します
// - count (int): 依存関係の数
//-----------------------------------------------------------------------------
void BaseJobTask::SetDependencyCount(const int count) {
	mRemainingDependencies_.store(count, std::memory_order_relaxed);
}

//-----------------------------------------------------------------------------
// Purpose: ジョブシステムを設定します
// - jobSystem (JobSystem*): ジョブシステム
//-----------------------------------------------------------------------------
void BaseJobTask::SetJobSystem(JobSystem* jobSystem) {
	mJobSystem_ = jobSystem;
}

//-----------------------------------------------------------------------------
// Purpose: 優先度を設定します
// - priority (int): 優先度
//-----------------------------------------------------------------------------
void BaseJobTask::SetPriority(const int priority) {
	mPriority_ = priority;
}
