#pragma once
#include <memory>

#include "TweenHandle.h"
#include "TweenInstance.h"

namespace Unnamed {
	/// @brief TweenManagerは、active Tweenをshared ownershipで保持し、更新・完了・一括停止を管理します
	class TweenManager {
	public:
		TweenManager()  = default;
		~TweenManager() = default;

		void Update(float deltaTime);

		void KillAll(bool complete = false);

		template <typename TValue>
		std::shared_ptr<TweenInstance<TValue>> Create(
			TweenInstance<TValue>::GetterFunc getter,
			TweenInstance<TValue>::SetterFunc setter,
			const TValue&                     endValue,
			float                             duration
		) {
			auto tween = std::make_shared<TweenInstance<TValue>>(
				std::move(getter), std::move(setter), endValue, duration
			);
			mTweens.emplace_back(tween);
			return tween;
		}

		template <typename TValue>
		TweenHandle To(
			TValue&       target,
			const TValue& endValue,
			float         duration
		) {
			auto tween = Create<TValue>(
				[&target]()-> TValue {
					return target;
				},
				[&target](const TValue& value) {
					target = value;
				},
				endValue,
				duration
			);

			return TweenHandle(tween);
		}

		template <typename TValue>
		std::shared_ptr<TweenInstance<TValue>> CreateTo(
			TValue&       target,
			const TValue& endValue,
			float         duration
		) {
			return Create<TValue>(
				[&target]() -> TValue {
					return target;
				},
				[&target](const TValue& value) {
					target = value;
				},
				endValue,
				duration
			);
		}

		template <typename TObject, typename TValue>
		TweenHandle ToMember(
			const std::weak_ptr<TObject>& object,
			TValue TObject::*             member,
			const TValue&                 endValue,
			float                         duration
		) {
			auto tween = Create<TValue>(
				[object, member]() -> TValue {
					const auto lockedObject = object.lock();
					if (!lockedObject) {
						return TValue{};
					}
					return lockedObject.get()->*member;
				},
				[object, member](const TValue& value) {
					const auto lockedObject = object.lock();
					if (!lockedObject) {
						return;
					}
					lockedObject.get()->*member = value;
				},
				endValue,
				duration
			);

			return TweenHandle(tween);
		}

	private:
		std::vector<std::shared_ptr<ITweenPlayable>> mTweens;
	};
}
