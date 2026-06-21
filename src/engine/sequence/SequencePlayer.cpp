#include "SequencePlayer.h"

#include <algorithm>

namespace Unnamed {
	SequencePlayer::SequencePlayer() {
		mPlayerId = sNextPlayerId++;
	}

	uint64_t SequencePlayer::GetPlayerId() const {
		return mPlayerId;
	}

	void SequencePlayer::SetAssetId(const AssetID assetId) {
		mAssetId            = assetId;
		mCurrentFrame       = 0.0f;
		mPreviousFrame      = 0.0f;
		mLastFrameRange     = {};
		mLastTraversalRange = {};
		mHasPendingSeek     = false;
		mCompiled.reset();
		mCompiledVersion = 0;
		mState           = SEQUENCE_PLAYER_STATE::STOPPED;
	}

	AssetID SequencePlayer::GetAssetId() const {
		return mAssetId;
	}

	void SequencePlayer::Play() {
		if (mAssetId == kInvalidAssetID) {
			return;
		}

		if (mState == SEQUENCE_PLAYER_STATE::COMPLETED) {
			if (
				mPlaybackDirection == SEQUENCE_PLAYBACK_DIRECTION::BACKWARD &&
				mCompiled
			) {
				mCurrentFrame = static_cast<float>(std::max<int64_t>(
					0,
					mCompiled->GetLengthFrames()
				));
			} else {
				mCurrentFrame = 0.0f;
			}
		}
		mState = SEQUENCE_PLAYER_STATE::PLAYING;
	}

	void SequencePlayer::Pause() {
		if (mState == SEQUENCE_PLAYER_STATE::PLAYING) {
			mState = SEQUENCE_PLAYER_STATE::PAUSED;
		}
	}

	void SequencePlayer::Stop() {
		mCurrentFrame       = 0.0f;
		mPreviousFrame      = 0.0f;
		mLastFrameRange     = {};
		mLastTraversalRange = {};
		mHasPendingSeek     = false;
		mState              = SEQUENCE_PLAYER_STATE::STOPPED;
	}

	void SequencePlayer::SeekFrames(const float frame) {
		const float clampedFrame = std::max(0.0f, frame);
		mPreviousFrame           = mCurrentFrame;
		mCurrentFrame            = clampedFrame;
		mLastFrameRange          = {};
		mLastTraversalRange      = {};
		mHasPendingSeek          = true;
	}

	void SequencePlayer::SetPlayRate(const float playRate) {
		mPlayRate = std::max(0.0f, playRate);
	}

	float SequencePlayer::GetPlayRate() const {
		return mPlayRate;
	}

	void SequencePlayer::SetPlaybackDirection(
		const SEQUENCE_PLAYBACK_DIRECTION direction
	) {
		mPlaybackDirection = direction;
	}

	SEQUENCE_PLAYBACK_DIRECTION SequencePlayer::GetPlaybackDirection() const {
		return mPlaybackDirection;
	}

	void SequencePlayer::SetSeekEventPolicy(
		const SEQUENCE_SEEK_EVENT_POLICY policy
	) {
		mSeekEventPolicy = policy;
	}

	SEQUENCE_SEEK_EVENT_POLICY SequencePlayer::GetSeekEventPolicy() const {
		return mSeekEventPolicy;
	}

	void SequencePlayer::SetLoop(const bool loopEnabled) {
		mLoopEnabled = loopEnabled;
	}

	bool SequencePlayer::GetLoop() const {
		return mLoopEnabled;
	}

	void SequencePlayer::SetWeight(const float weight) {
		mWeight = std::clamp(weight, 0.0f, 1.0f);
	}

	float SequencePlayer::GetWeight() const {
		return mWeight;
	}

	void SequencePlayer::SetCompletionMode(
		const SEQUENCE_COMPLETION_MODE mode
	) {
		mCompletionMode = mode;
	}

	SEQUENCE_COMPLETION_MODE SequencePlayer::GetCompletionMode() const {
		return mCompletionMode;
	}

	float SequencePlayer::GetCurrentFrame() const {
		return mCurrentFrame;
	}

	float SequencePlayer::GetPreviousFrame() const {
		return mPreviousFrame;
	}

	SEQUENCE_PLAYER_STATE SequencePlayer::GetState() const {
		return mState;
	}

	SequenceFrameRange SequencePlayer::GetLastFrameRange() const {
		return mLastFrameRange;
	}

	SequenceTraversalRange SequencePlayer::GetLastTraversalRange() const {
		return mLastTraversalRange;
	}

	bool SequencePlayer::IsPlaying() const {
		return mState == SEQUENCE_PLAYER_STATE::PLAYING;
	}

	bool SequencePlayer::IsEvaluating() const {
		if (
			mState == SEQUENCE_PLAYER_STATE::PLAYING ||
			mState == SEQUENCE_PLAYER_STATE::PAUSED
		) {
			return true;
		}

		return
			mState == SEQUENCE_PLAYER_STATE::COMPLETED &&
			mCompletionMode == SEQUENCE_COMPLETION_MODE::KEEP_STATE;
	}

	bool SequencePlayer::ShouldSuppressEventsThisTick() const {
		if (!mLastTraversalRange.valid) {
			return true;
		}
		return
			mLastTraversalRange.causedBySeek &&
			mSeekEventPolicy == SEQUENCE_SEEK_EVENT_POLICY::SUPPRESS;
	}

	void SequencePlayer::AdvanceFrame(
		const float             deltaSeconds,
		const CompiledSequence& compiled
	) {
		mLastFrameRange             = {};
		mLastTraversalRange         = {};
		const float frameBeforeTick = mCurrentFrame;
		mPreviousFrame              = frameBeforeTick;

		// シーク要求がある場合はまずシーク区間を確定し、次フレーム評価へ引き渡します。
		if (mHasPendingSeek) {
			mLastTraversalRange.startFrame = mPreviousFrame;
			mLastTraversalRange.endFrame   = mCurrentFrame;
			mLastTraversalRange.direction  = mCurrentFrame < mPreviousFrame ?
				                                SEQUENCE_PLAYBACK_DIRECTION::BACKWARD :
				                                SEQUENCE_PLAYBACK_DIRECTION::FORWARD;
			mLastTraversalRange.causedBySeek = true;
			mLastTraversalRange.valid = mCurrentFrame != mPreviousFrame;
			mLastTraversalRange.traversalSerial = ++mTraversalSerialCounter;
			mHasPendingSeek = false;

			if (mLastTraversalRange.valid) {
				mLastFrameRange = SequenceFrameRange{
					.startFrame = mLastTraversalRange.startFrame,
					.endFrame   = mLastTraversalRange.endFrame,
					.valid      = true,
				};
			}
			return;
		}

		// 再生中でない場合も現在値評価は行うため、区間だけ無効にします。
		if (mState != SEQUENCE_PLAYER_STATE::PLAYING) {
			return;
		}

		const int64_t lengthFrames = std::max<int64_t>(
			0, compiled.GetLengthFrames());
		if (lengthFrames <= 0) {
			mCurrentFrame = 0.0f;
			mState        = SEQUENCE_PLAYER_STATE::COMPLETED;
			return;
		}

		const float frameDelta = deltaSeconds * static_cast<float>(
			                         compiled.GetAsset()->tickResolution
		                         ) * std::max(0.0f, mPlayRate);
		if (frameDelta <= 0.0f) {
			return;
		}

		const float length    = static_cast<float>(lengthFrames);
		float       nextFrame = mCurrentFrame;
		bool        wrapped   = false;
		int32_t     loopCount = 0;
		if (mPlaybackDirection == SEQUENCE_PLAYBACK_DIRECTION::FORWARD) {
			nextFrame += frameDelta;
			if (!mLoopEnabled) {
				if (nextFrame >= length) {
					nextFrame = length;
					mState    = SEQUENCE_PLAYER_STATE::COMPLETED;
				}
			} else {
				while (nextFrame > length) {
					nextFrame -= length;
					wrapped   = true;
					++loopCount;
				}
			}
		} else {
			nextFrame -= frameDelta;
			if (!mLoopEnabled) {
				if (nextFrame <= 0.0f) {
					nextFrame = 0.0f;
					mState    = SEQUENCE_PLAYER_STATE::COMPLETED;
				}
			} else {
				while (nextFrame < 0.0f) {
					nextFrame += length;
					wrapped   = true;
					++loopCount;
				}
			}
		}

		nextFrame     = std::clamp(nextFrame, 0.0f, length);
		mCurrentFrame = nextFrame;
		if (mCurrentFrame == mPreviousFrame) {
			return;
		}

		mLastTraversalRange = SequenceTraversalRange{
			.startFrame      = mPreviousFrame,
			.endFrame        = mCurrentFrame,
			.direction       = mPlaybackDirection,
			.wrapped         = wrapped,
			.loopCount       = loopCount,
			.causedBySeek    = false,
			.traversalSerial = ++mTraversalSerialCounter,
			.valid           = true,
		};
		mLastFrameRange = SequenceFrameRange{
			.startFrame = mLastTraversalRange.startFrame,
			.endFrame   = mLastTraversalRange.endFrame,
			.valid      = true,
		};
	}

	void SequencePlayer::ClearEventSuppression() {
		// シーク方針で制御するため、v2では明示クリア不要です。
	}

	void SequencePlayer::ClampCurrentFrame(const float maxFrame) {
		const float clamped = std::clamp(mCurrentFrame, 0.0f,
		                                 std::max(0.0f, maxFrame));
		mCurrentFrame       = clamped;
		mPreviousFrame      = clamped;
		mLastFrameRange     = {};
		mLastTraversalRange = {};
		mHasPendingSeek     = false;
	}

	void SequencePlayer::SetCompiled(
		std::shared_ptr<const SequenceAssetData> asset,
		const uint64_t                           version
	) {
		mCompiled        = std::make_unique<CompiledSequence>(std::move(asset));
		mCompiledVersion = version;
	}

	const CompiledSequence* SequencePlayer::GetCompiled() const {
		return mCompiled.get();
	}

	uint64_t SequencePlayer::GetCompiledVersion() const {
		return mCompiledVersion;
	}

	uint64_t SequencePlayer::sNextPlayerId = 1;
}
