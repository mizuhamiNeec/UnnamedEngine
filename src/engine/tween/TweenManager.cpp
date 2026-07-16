#include "TweenManager.h"

namespace Unnamed {
	void TweenManager::Update(const float deltaTime) {
		for (const auto& tween : mTweens) {
			if (!tween) {
				continue;
			}

			// 更新
			tween->Update(deltaTime);
		}

		// 完了したTweenはリストから削除する
		std::erase_if(
			mTweens,
			[](const std::shared_ptr<ITweenPlayable>& tween) {
				return !tween || !tween->IsAlive();
			}
		);
	}

	void TweenManager::KillAll(const bool complete) {
		for (const auto& tween : mTweens) {
			if (!tween) {
				continue;
			}

			// 更新
			tween->Kill(complete);
		}

		// 完了したTweenはリストから削除する
		std::erase_if(
			mTweens,
			[](const std::shared_ptr<ITweenPlayable>& tween) {
				return !tween || !tween->IsAlive();
			}
		);
	}
}
