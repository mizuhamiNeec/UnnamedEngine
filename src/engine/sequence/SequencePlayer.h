#pragma once

#include <cstdint>
#include <memory>

#include "core/assets/AssetID.h"

#include "CompiledSequence.h"
#include "SequenceRuntimeTypes.h"

namespace Unnamed {
	/// @brief シーケンス再生区間です。
	struct SequenceFrameRange final {
		float startFrame = 0.0f;
		float endFrame   = 0.0f;
		bool  valid      = false;
	};

	/// @brief シーケンス再生を保持するランタイムプレイヤーです。
	class SequencePlayer final {
	public:
		/// @brief コンストラクタです。
		SequencePlayer();

		/// @brief プレイヤーIDを取得します。
		[[nodiscard]] uint64_t GetPlayerId() const;

		/// @brief 再生対象アセットIDを設定します。
		void SetAssetId(AssetID assetId);

		/// @brief 再生対象アセットIDを取得します。
		[[nodiscard]] AssetID GetAssetId() const;

		/// @brief 再生を開始します。
		void Play();

		/// @brief 再生を一時停止します。
		void Pause();

		/// @brief 再生を停止します。
		void Stop();

		/// @brief 指定フレームへシークします。
		void SeekFrames(float frame);

		/// @brief 再生速度を設定します。
		void SetPlayRate(float playRate);

		/// @brief 再生速度を取得します。
		[[nodiscard]] float GetPlayRate() const;

		/// @brief 再生方向を設定します。
		void SetPlaybackDirection(SEQUENCE_PLAYBACK_DIRECTION direction);

		/// @brief 再生方向を取得します。
		[[nodiscard]] SEQUENCE_PLAYBACK_DIRECTION GetPlaybackDirection() const;

		/// @brief シーク時イベント方針を設定します。
		void SetSeekEventPolicy(SEQUENCE_SEEK_EVENT_POLICY policy);

		/// @brief シーク時イベント方針を取得します。
		[[nodiscard]] SEQUENCE_SEEK_EVENT_POLICY GetSeekEventPolicy() const;

		/// @brief ループ再生を設定します。
		void SetLoop(bool loopEnabled);

		/// @brief ループ再生が有効かを取得します。
		[[nodiscard]] bool GetLoop() const;

		/// @brief プレイヤーウェイトを設定します。
		void SetWeight(float weight);

		/// @brief プレイヤーウェイトを取得します。
		[[nodiscard]] float GetWeight() const;

		/// @brief 完了時モードを設定します。
		void SetCompletionMode(SEQUENCE_COMPLETION_MODE mode);

		/// @brief 完了時モードを取得します。
		[[nodiscard]] SEQUENCE_COMPLETION_MODE GetCompletionMode() const;

		/// @brief 現在フレームを取得します。
		[[nodiscard]] float GetCurrentFrame() const;

		/// @brief 1フレーム前のフレームを取得します。
		[[nodiscard]] float GetPreviousFrame() const;

		/// @brief 現在の再生状態を取得します。
		[[nodiscard]] SEQUENCE_PLAYER_STATE GetState() const;

		/// @brief 現在フレームの進行区間を取得します。
		[[nodiscard]] SequenceFrameRange GetLastFrameRange() const;

		/// @brief 現在フレームの走査情報を取得します。
		[[nodiscard]] SequenceTraversalRange GetLastTraversalRange() const;

		/// @brief 再生中かどうかを返します。
		[[nodiscard]] bool IsPlaying() const;

		/// @brief 現在評価対象として有効かどうかを返します。
		[[nodiscard]] bool IsEvaluating() const;

		/// @brief 前進区間に対するイベント評価を抑制するかを返します。
		[[nodiscard]] bool ShouldSuppressEventsThisTick() const;

		/// @brief 1フレーム分の再生時間を進めます。
		void AdvanceFrame(float deltaSeconds, const CompiledSequence& compiled);

		/// @brief イベント抑制フラグをクリアします。
		void ClearEventSuppression();

		/// @brief 現在フレームをシーケンス長へ収めます。
		void ClampCurrentFrame(float maxFrame);

		/// @brief コンパイル済みアセットを更新します。
		void SetCompiled(
			std::shared_ptr<const SequenceAssetData> asset, uint64_t version
		);

		/// @brief 現在のコンパイル済みデータを取得します。
		[[nodiscard]] const CompiledSequence* GetCompiled() const;

		/// @brief 現在のコンパイル済みアセットバージョンを取得します。
		[[nodiscard]] uint64_t GetCompiledVersion() const;

	private:
		static uint64_t sNextPlayerId;

		uint64_t mPlayerId = 0;
		AssetID  mAssetId  = kInvalidAssetID;

		SEQUENCE_PLAYER_STATE    mState = SEQUENCE_PLAYER_STATE::STOPPED;
		SEQUENCE_COMPLETION_MODE mCompletionMode =
			SEQUENCE_COMPLETION_MODE::RESTORE_STATE;
		SEQUENCE_PLAYBACK_DIRECTION mPlaybackDirection =
			SEQUENCE_PLAYBACK_DIRECTION::FORWARD;
		SEQUENCE_SEEK_EVENT_POLICY mSeekEventPolicy =
			SEQUENCE_SEEK_EVENT_POLICY::SUPPRESS;
		bool                   mLoopEnabled            = false;
		float                  mPlayRate               = 1.0f;
		float                  mWeight                 = 1.0f;
		float                  mCurrentFrame           = 0.0f;
		float                  mPreviousFrame          = 0.0f;
		SequenceFrameRange     mLastFrameRange         = {};
		SequenceTraversalRange mLastTraversalRange     = {};
		bool                   mHasPendingSeek         = false;
		uint64_t               mTraversalSerialCounter = 0;

		std::unique_ptr<CompiledSequence> mCompiled        = nullptr;
		uint64_t                          mCompiledVersion = 0;
	};
}
