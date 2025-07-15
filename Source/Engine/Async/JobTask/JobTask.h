#pragma once
#include <future>

#include <Async/JobTask/Base/BaseJobTask.h>

template<typename ReturnType>
class JobTask : public BaseJobTask {
public:
	explicit JobTask(std::packaged_task<ReturnType()>&& task);

	void Execute() override;

	std::future<ReturnType> GetFuture();

private:
	std::packaged_task<ReturnType()> mTask_;
	std::future<ReturnType> mFuture_;
};

template <typename ReturnType>
JobTask<ReturnType>::JobTask(std::packaged_task<ReturnType()>&& task)
	: mTask_(std::move(task)), mFuture_(mTask_.get_future()) {
}

template <typename ReturnType>
void JobTask<ReturnType>::Execute() {
	mTask_();
}

template <typename ReturnType>
std::future<ReturnType> JobTask<ReturnType>::GetFuture() {
	return std::move(mFuture_);
}