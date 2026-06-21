#include "TweenHandle.h"

namespace Unnamed {
	TweenHandle::TweenHandle() = default;

	TweenHandle::TweenHandle(
		const std::weak_ptr<ITweenPlayable>& playable
	) : mPlayable(playable) {
	}

	bool TweenHandle::IsValid() const {
		return !mPlayable.expired();
	}

	bool TweenHandle::IsPlaying() const {
		const auto playable = mPlayable.lock();
		if (!playable) {
			return false;
		}
		return playable->IsPlaying();
	}

	bool TweenHandle::IsComplete() const {
		const auto playable = mPlayable.lock();
		if (!playable) {
			return false;
		}
		return playable->IsComplete();
	}

	void TweenHandle::Pause() {
		const auto playable = mPlayable.lock();
		if (!playable) {
			return;
		}
		playable->Pause();
	}

	void TweenHandle::Resume() const {
		const auto playable = mPlayable.lock();
		if (!playable) {
			return;
		}
		playable->Resume();
	}

	void TweenHandle::Kill(const bool complete) const {
		const auto playable = mPlayable.lock();
		if (!playable) {
			return;
		}
		playable->Kill(complete);
	}
}
