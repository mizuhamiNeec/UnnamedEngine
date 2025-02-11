#include "MainThreadDispatcher.h"

//-----------------------------------------------------------------------------
// Purpose: タスクをキューに追加します
// - task (std::function<void()>): タスク
//-----------------------------------------------------------------------------
void MainThreadDispatcher::Enqueue(std::function<void()> task) {
	std::lock_guard lock(mMutex_);
	mTasks_.push(std::move(task));
}

//-----------------------------------------------------------------------------
// Purpose: すべてのタスクを処理します
//-----------------------------------------------------------------------------
void MainThreadDispatcher::ProcessAll() {
	std::queue<std::function<void()>> tasks;
	{
		std::lock_guard lock(mMutex_);
		std::swap(tasks, mTasks_);
	}
	while (!tasks.empty()) {
		tasks.front()();
		tasks.pop();
	}
}
