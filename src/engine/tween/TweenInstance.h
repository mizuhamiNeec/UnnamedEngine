#pragma once
#include <algorithm>
#include <functional>
#include <utility>

#include "core/math/Math.h"

#include "ITweenPlayable.h"
#include "TweenEase.h"
#include "TweenLerp.h"
#include "TweenTypes.h"

namespace Unnamed {
	/// @brief Tweenのインスタンスを表すクラス。指定されたGetter関数とSetter関数を使用して、値の補間を行います。
	/// @tparam TValue 補間する値の型。float、Vec2、Vec3、Vec4、Quaternionに対応。
	template <typename TValue>
	class TweenInstance final : public ITweenPlayable {
	public:
		using GetterFunc   = std::function<TValue()>;
		using SetterFunc   = std::function<void(const TValue&)>;
		using CompleteFunc = std::function<void()>;

		TweenInstance(
			GetterFunc getter,
			SetterFunc setter,
			TValue     endValue,
			float      duration
		);

		/// @brief イージング関数の種類を設定します。イージング関数は、Tweenの進行に対する値の変化の仕方を定義します。
		/// @param easeType イージング関数の種類。
		TweenInstance& SetEase(EASE_TYPE easeType);

		/// @brief カスタムのイージング関数を設定します。
		/// @param easeFunc 正規化時間(0.0f ~ 1.0f)を受け取り、補間係数を返す関数。
		TweenInstance& SetEaseFunc(std::function<float(float)> easeFunc);

		/// @brief Cubic Bezier によるイージング関数を設定します。
		/// @param p1 制御点1
		/// @param p2 制御点2
		TweenInstance& SetEaseCubicBezier(const Vec2& p1, const Vec2& p2);

		/// @brief Tweenの開始前の遅延時間を設定します。遅延時間はTweenの開始前に待機する時間で、0以上の値を指定できます。
		/// @param delay 遅延時間（秒）。0以上の値を指定してください。
		TweenInstance& SetDelay(float delay);

		/// @brief ループ設定を行います
		/// @param loopCount ループ回数。0でループなし、-1で無限ループになります。
		/// @param loopType ループタイプ。
		TweenInstance& SetLoops(int loopCount, LOOP_TYPE loopType);

		/// @brief Tweenの完了時に呼び出されるコールバック関数を設定します
		/// @param onComplete Tweenの完了時に呼び出される関数。引数なし。
		TweenInstance& OnComplete(CompleteFunc onComplete);

		/// @brief Tweenを更新します。Tweenの状態に応じて、遅延のカウントダウン、値の補間、ループや完了の処理を行います。
		/// @param deltaTime 前のフレームからの経過時間（秒）
		void Update(float deltaTime) override;

		/// @brief Tweenの再生を一時停止します。Tweenが遅延中であっても、遅延状態のまま一時停止されます。
		void Pause() override;

		/// @brief Tweenの再生を再開します。Tweenが遅延中であれば遅延状態に、そうでなければ通常再生状態に戻ります。
		void Resume() override;

		/// @brief Tweenを終了させます。completeがtrueの場合はTweenを完了状態にし、falseの場合はTweenを即座に停止します。
		/// @param complete trueの場合、Tweenを完了状態にしてから終了します。falseの場合、Tweenを即座に終了します。
		void Kill(bool complete) override;

		/// @brief Tweenがまだ有効であるかどうかを判定します。Tweenが有効であるとは、Tweenが終了していない状態を指します。
		/// @return 有効な場合はtrue、そうでない場合はfalse
		[[nodiscard]] bool IsAlive() const override;

		/// @brief Tweenが再生中かどうかを判定します。再生中とは、Tweenが遅延中または通常再生中である状態を指します。
		/// @return 再生中の場合はtrue、そうでない場合はfalse
		[[nodiscard]] bool IsPlaying() const override;

		/// @brief Tweenが完了しているかどうかを判定します。完了状態は、Tweenが正常に終了した場合だけでなく、ループ回数が尽きた場合も含みます。
		/// @return 完了している場合はtrue、そうでない場合はfalse
		[[nodiscard]] bool IsComplete() const override;

	private:
		/// @brief Getter関数を呼び出して現在の値を取得し、開始値として保存します。
		void CaptureStartValue();

		/// @brief 現在の経過時間に基づいて補間値を計算し、Setter関数を呼び出して適用します。
		/// @param t 正規化された時間（0.0fから1.0fの範囲）。イージング関数が適用された後の値を使用します。
		void ApplyValue(float t);

		/// @brief 現在の再生方向に応じた最終値を適用します。逆再生中なら開始値を、通常再生中なら終了値を適用します。
		void ApplyFinalValueForCurrentDirection();

		/// @brief ループ処理または完了処理を行います。ループ回数が残っていればループし、そうでなければ完了させます。
		void HandleLoopOrComplete();

		/// @brief Tweenを完了状態にし、必要に応じてコールバック関数を呼び出します。Tweenの状態をCOMPLETEDに設定し、再生中であれば最終値を適用します。
		void CompleteInternal();

		GetterFunc   mGetter;
		SetterFunc   mSetter;
		CompleteFunc mOnComplete;

		TValue mStartValue = {};
		TValue mEndValue   = {};

		float mDuration    = 0.0f;
		float mElapsedTime = 0.0f;

		float mDelay          = 0.0f;
		float mDelayRemaining = 0.0f;

		int  mLoopCount  = 0;
		bool mIsReversed = false;
		bool mHasStarted = false;
		bool mIsAlive    = true;

		EASE_TYPE                   mEaseType = EASE_TYPE::LINEAR;
		std::function<float(float)> mEaseFunc = {};
		LOOP_TYPE                   mLoopType = LOOP_TYPE::RESTART;
		TWEEN_STATE                 mState    = TWEEN_STATE::PLAYING;
	};

	template <typename TValue>
	TweenInstance<TValue>::TweenInstance(
		GetterFunc  getter, SetterFunc setter, TValue endValue,
		const float duration
	) : mGetter(getter),
	    mSetter(setter),
	    mEndValue(endValue),
	    mDuration(duration) {
		mState = TWEEN_STATE::PLAYING;
	}

	template <typename TValue>
	TweenInstance<TValue>& TweenInstance<TValue>::SetEase(
		const EASE_TYPE easeType
	) {
		mEaseType = easeType;
		mEaseFunc = {};
		return *this;
	}

	template <typename TValue>
	TweenInstance<TValue>& TweenInstance<TValue>::SetEaseFunc(
		std::function<float(float)> easeFunc
	) {
		mEaseFunc = std::move(easeFunc);
		return *this;
	}

	template <typename TValue>
	TweenInstance<TValue>& TweenInstance<TValue>::SetEaseCubicBezier(
		const Vec2& p1, const Vec2& p2
	) {
		return SetEaseFunc(
			[p1, p2](const float t) {
				return Math::CubicBezier(t, p1, p2);
			}
		);
	}

	template <typename TValue>
	TweenInstance<TValue>& TweenInstance<TValue>::SetDelay(const float delay) {
		mDelay          = std::max(delay, 0.0f);
		mDelayRemaining = mDelay;
		if (mDelayRemaining > 0.0f) {
			mState = TWEEN_STATE::DELAYED;
		}
		return *this;
	}

	template <typename TValue>
	TweenInstance<TValue>& TweenInstance<TValue>::SetLoops(
		const int loopCount, const LOOP_TYPE loopType
	) {
		mLoopCount = loopCount;
		mLoopType  = loopType;
		return *this;
	}

	template <typename TValue>
	TweenInstance<TValue>& TweenInstance<TValue>::OnComplete(
		CompleteFunc onComplete
	) {
		mOnComplete = std::move(onComplete);
		return *this;
	}

	template <typename TValue>
	void TweenInstance<TValue>::Update(const float deltaTime) {
		if (!mIsAlive) {
			return;
		}
		if (
			mState == TWEEN_STATE::PAUSED ||
			mState == TWEEN_STATE::COMPLETED ||
			mState == TWEEN_STATE::KILLED
		) {
			return;
		}

		if (mState == TWEEN_STATE::DELAYED) {
			mDelayRemaining -= deltaTime;
			if (mDelayRemaining > 0.0f) {
				return;
			}

			mState = TWEEN_STATE::PLAYING;
		}

		if (!mHasStarted) {
			CaptureStartValue();
			ApplyValue(0.0f);
			mHasStarted = true;
		}

		mElapsedTime += deltaTime;

		const float normalizedTime = mDuration <= 0.0f ?
			                             1.0f :
			                             std::clamp(
				                             mElapsedTime / mDuration,
				                             0.0f,
				                             1.0f
			                             );
		const float effectiveTime = mIsReversed ?
			                            1.0f - normalizedTime :
			                            normalizedTime;
		const float easedTime = mEaseFunc ?
			                        std::clamp(
				                        mEaseFunc(effectiveTime),
				                        0.0f,
				                        1.0f
			                        ) :
			                        TweenEase::Evaluate(
				                        mEaseType, effectiveTime
			                        );

		ApplyValue(easedTime);

		if (mElapsedTime < mDuration) {
			return;
		}

		HandleLoopOrComplete();
	}

	template <typename TValue>
	void TweenInstance<TValue>::Pause() {
		if (!mIsAlive) {
			return;
		}

		if (
			mState == TWEEN_STATE::PLAYING ||
			mState == TWEEN_STATE::DELAYED
		) {
			mState = TWEEN_STATE::PAUSED;
		}
	}

	template <typename TValue>
	void TweenInstance<TValue>::Resume() {
		if (!mIsAlive) {
			return;
		}
		if (mState == TWEEN_STATE::PAUSED) {
			mState = mDelayRemaining > 0.0f ?
				         TWEEN_STATE::DELAYED :
				         TWEEN_STATE::PLAYING;
		}
	}

	template <typename TValue>
	void TweenInstance<TValue>::Kill(const bool complete) {
		if (!mIsAlive) {
			return;
		}

		if (complete) {
			if (!mHasStarted) {
				CaptureStartValue();
				mHasStarted = true;
			}

			ApplyFinalValueForCurrentDirection();
			CompleteInternal();
			return;
		}

		mState   = TWEEN_STATE::KILLED;
		mIsAlive = false;
	}

	template <typename TValue>
	bool TweenInstance<TValue>::IsAlive() const {
		return mIsAlive;
	}

	template <typename TValue>
	bool TweenInstance<TValue>::IsPlaying() const {
		return mIsAlive &&
		       (
			       mState == TWEEN_STATE::PLAYING ||
			       mState == TWEEN_STATE::DELAYED
		       );
	}

	template <typename TValue>
	bool TweenInstance<TValue>::IsComplete() const {
		return mState == TWEEN_STATE::COMPLETED;
	}

	template <typename TValue>
	void TweenInstance<TValue>::CaptureStartValue() {
		mStartValue = mGetter();
	}

	template <typename TValue>
	void TweenInstance<TValue>::ApplyValue(float t) {
		const TValue value = TweenLerp<TValue>::Evaluate(
			mStartValue, mEndValue, t
		);
		mSetter(value);
	}

	template <typename TValue>
	void TweenInstance<TValue>::ApplyFinalValueForCurrentDirection() {
		if (mIsReversed) {
			// 逆再生中なら開始値を適用
			mSetter(mStartValue);
		} else {
			// 通常再生中なら終了値を適用
			mSetter(mEndValue);
		}
	}

	template <typename TValue>
	void TweenInstance<TValue>::HandleLoopOrComplete() {
		// ループ回数が残っている場合はループ処理を行う
		ApplyFinalValueForCurrentDirection();

		// ループ回数が0の場合は完了させる
		if (mLoopCount == 0) {
			CompleteInternal();
			return;
		}

		// ループ回数が正の値の場合は減らす。
		if (mLoopCount > 0) {
			--mLoopCount;
		}

		// ループタイプに応じて再生方向を切り替え、時間をリセット
		mElapsedTime = 0.0f;

		// YOYOループの場合は再生方向を反転
		if (mLoopType == LOOP_TYPE::YOYO) {
			mIsReversed = !mIsReversed;
		}
	}

	template <typename TValue>
	void TweenInstance<TValue>::CompleteInternal() {
		mState   = TWEEN_STATE::COMPLETED;
		mIsAlive = false;

		if (mOnComplete) {
			mOnComplete();
		}
	}
}
