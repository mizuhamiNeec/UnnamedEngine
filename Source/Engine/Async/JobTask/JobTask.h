#pragma once
#include <future>

#include <Async/JobTask/Base/BaseJobTask.h>

template<typename ReturnType>
class JobTask: public BaseJobTask {
public:
	explicit JobTask(std::packaged_task<ReturnType()>&& task);

	void Execute() override;

	std::future<ReturnType> GetFuture();

private:
	std::packaged_task<ReturnType()> mTask;
	std::future<ReturnType> mFuture;
};

template <typename ReturnType>
JobTask<ReturnType>::JobTask(std::packaged_task<ReturnType()>&& task)
	: mTask(std::move(task)),mFuture(mTask.get_future()) {}

template <typename ReturnType>
void JobTask<ReturnType>::Execute() {
	mTask();
}

template <typename ReturnType>
std::future<ReturnType> JobTask<ReturnType>::GetFuture() {
	return std::move(mFuture);
}