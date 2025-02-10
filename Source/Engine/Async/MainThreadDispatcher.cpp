#include "MainThreadDispatcher.h"

void MainThreadDispatcher::Enqueue(std::function<void()> task) {
	std::lock_guard lock(mMutex);
	mTasks.push(std::move(task));
}

void MainThreadDispatcher::ProcessAll() {
	std::queue<std::function<void()>> tasks;
	{
		std::lock_guard lock(mMutex);
		std::swap(tasks,mTasks);
	}
	while(!tasks.empty()) {
		tasks.front()();
		tasks.pop();
	}
}
