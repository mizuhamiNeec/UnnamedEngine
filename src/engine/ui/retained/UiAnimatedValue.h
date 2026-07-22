#pragma once
#include <algorithm>

namespace Unnamed::UI::Retained {
	template <typename T>
	T InterpolateUiValue(
		const T& start,
		const T& target,
		float    time
	) {
		return start + (target - start) * time;
	}

	template <typename T>
	class UiAnimatedValue {
	public:
		void SetImmediate(const T& value) noexcept {
			mCurrentValue = value;
			mStartValue   = value;
			mTargetValue  = value;

			mElapsedSeconds  = 0.0f;
			mDurationSeconds = 0.0f;
			mAnimating       = false;
		}

		void AnimateTo(
			const T&    target,
			const float duration
		) {
			if (duration <= 0.0f) {
				SetImmediate(target);
				return;
			}

			mStartValue  = mCurrentValue;
			mTargetValue = target;

			mElapsedSeconds  = 0.0f;
			mDurationSeconds = duration;
			mAnimating       = true;
		}

		void Update(const float deltaTime) noexcept {
			if (!mAnimating) {
				return;
			}

			mElapsedSeconds += std::max(deltaTime, 0.0f);

			const float normalizedTime = std::clamp(
				mElapsedSeconds / mDurationSeconds,
				0.0f,
				1.0f
			);

			const float easedTime = normalizedTime; // TODO: 一旦リニアで

			mCurrentValue = InterpolateUiValue(mStartValue, mTargetValue,
			                                   easedTime);

			if (normalizedTime >= 1.0f) {
				mCurrentValue = mTargetValue;
				mAnimating    = false;
			}
		}

		[[nodiscard]]
		const T& Value() const noexcept {
			return mCurrentValue;
		}

		[[nodiscard]]
		bool IsAnimating() const noexcept {
			return mAnimating;
		}

	private:
		T mCurrentValue = {};
		T mStartValue   = {};
		T mTargetValue  = {};

		float mElapsedSeconds  = 0.0f;
		float mDurationSeconds = 0.0f;

		bool mAnimating = false;
	};
}
