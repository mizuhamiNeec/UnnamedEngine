#pragma once
#include <functional>
#include <mutex>
#include <queue>

class MainThreadDispatcher {
public:
	void Enqueue(std::function<void()> task);
	void ProcessAll();

private:
	std::mutex mMutex;
	std::queue<std::function<void()>> mTasks;
};
