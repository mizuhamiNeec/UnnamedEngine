#pragma once
#include <memory>

#include "TweenHandle.h"
#include "TweenInstance.h"

namespace Unnamed {
	/// @brief TweenManagerは、active Tweenをshared ownershipで保持し、更新・完了・一括停止を管理します
	class TweenManager {
	public:
		TweenManager() = default;

		/// @brief 更新処理
		/// @param deltaTime 前のフレームからの経過時間（秒）
		void Update(float deltaTime);

		/// @brief すべてのTweenを終了します。
		/// @param complete trueの場合、Tweenを完了状態にしてから終了します。falseの場合、Tweenを即座に終了します。
		void KillAll(bool complete = false);

		/// @brief Tweenを作成し追加します。
		/// @param getter 取得関数
		/// @param setter 設定関数
		/// @param endValue 目標値
		/// @param duration Tweenの継続時間（秒）
		/// @return 作成されたTweenのインスタンスへのshared_ptr
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

		/// @brief 指定された変数に対してTweenを作成し、目標値まで補間します。
		/// @tparam TValue 補間する値の型。float、Vec2、Vec3、Vec4、Quaternionに対応。
		/// @param target 補間対象の変数への参照
		/// @param endValue 目標値
		/// @param duration Tweenの継続時間（秒）
		/// @return TweenHandle。Tweenの操作に使用できます。
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

		/// @brief 指定された変数に対してTweenを作成し、目標値まで補間します。
		/// @tparam TValue 補間する値の型。float、Vec2、Vec3、Vec4、Quaternionに対応。
		/// @param target 補間対象の変数への参照
		/// @param endValue 目標値
		/// @param duration Tweenの継続時間（秒）
		/// @return TweenHandle。Tweenの操作に使用できます。
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

		/// @brief 指定されたオブジェクトのメンバー変数に対してTweenを作成し、目標値まで補間します。
		/// @tparam TObject 補間対象のオブジェクトの型
		/// @tparam TValue 補間する値の型。float、Vec2、Vec3、Vec4、Quaternionに対応。
		/// @param object 補間対象のオブジェクトへのweak_ptr
		/// @param member 補間対象のメンバー変数へのポインタ
		/// @param endValue 目標値
		/// @param duration Tweenの継続時間（秒）
		/// @return TweenHandle。Tweenの操作に使用できます。
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
