#include "Profiler.h"

#include <algorithm>

namespace Unnamed {
	Profiler::ScopeTimer::ScopeTimer(
		Profiler* profiler, const std::string_view name
	) : mProfiler(profiler),
	    mName(name),
	    mStart(Clock::now()) {
	}

	Profiler::ScopeTimer::~ScopeTimer() {
		if (!mProfiler) {
			return;
		}
		const auto  end       = Clock::now();
		const float elapsedMs = std::chrono::duration<float, std::milli>(
			end - mStart
		).count();
		mProfiler->AddSample(mName, elapsedMs);
	}

	void Profiler::BeginFrame() {
		for (auto& sample : mSamples) {
			sample.frameAccumulatedMs = 0.0f;
		}
	}

	void Profiler::EndFrame() {
		for (auto& sample : mSamples) {
			if (sample.history.empty()) {
				sample.history.resize(kHistorySize, 0.0f);
			}

			sample.currentMs                   = sample.frameAccumulatedMs;
			sample.history[mHistoryWriteIndex] = sample.currentMs;

			float sumMs = 0.0f;
			float maxMs = 0.0f;
			for (const float value : sample.history) {
				sumMs = sumMs + value;
				maxMs = std::max(maxMs, value);
			}

			sample.averageMs = sumMs / static_cast<float>(sample.history.
				                   size());
			sample.maxMs = maxMs;
		}

		mHistoryWriteIndex = (mHistoryWriteIndex + 1u) % kHistorySize;
		++mFrameCount;
	}

	void Profiler::AddSample(
		const std::string_view name, const float milliseconds
	) {
		GetOrCreateSample(name).frameAccumulatedMs += milliseconds;
	}

	const std::vector<Profiler::SampleView>& Profiler::GetSamples() const {
		mSampleViews.clear();
		mSampleViews.reserve(mSamples.size());
		for (const auto& sample : mSamples) {
			mSampleViews.emplace_back(
				SampleView{
					.name              = sample.name,
					.history           = &sample.history,
					.historyWriteIndex = mHistoryWriteIndex,
					.currentMs         = sample.currentMs,
					.averageMs         = sample.averageMs,
					.maxMs             = sample.maxMs,
					.colorIndex        = sample.colorIndex,
				}
			);
		}
		return mSampleViews;
	}

	uint32_t Profiler::GetHistorySize() const {
		return kHistorySize;
	}

	uint64_t Profiler::GetFrameCount() const {
		return mFrameCount;
	}

	Profiler::SampleData& Profiler::GetOrCreateSample(
		const std::string_view name
	) {
		const std::string key(name);
		if (const auto it = mSampleIndices.find(key);
			it != mSampleIndices.end()) {
			return mSamples[it->second];
		}

		SampleData data = {};
		data.name       = key;
		data.history.resize(kHistorySize, 0.0f);
		data.colorIndex = static_cast<uint32_t>(mSamples.size());

		const size_t index = mSamples.size();
		mSampleIndices.emplace(data.name, index);
		mSamples.emplace_back(std::move(data));
		return mSamples.back();
	}
}
