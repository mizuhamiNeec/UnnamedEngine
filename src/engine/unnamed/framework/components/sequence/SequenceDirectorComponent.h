#pragma once

#include <memory>
#include <string>
#include <vector>

#include "engine/sequence/SequenceRuntimeTypes.h"
#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;
	class SequencePlayer;

	/// @brief Sequence再生要求とロック対象管理を行うコンポーネントです。
	class SequenceDirectorComponent final : public BaseComponent {
	public:
		/// @brief コンポーネント追加時の初期化を行います。
		void OnAttached() override;
		/// @brief コンポーネント破棄直前の終了処理を行います。
		void OnDetached() override;
		/// @brief ゲーム中の再生制御更新を行います。
		void OnTick(float deltaTime) override;
		/// @brief エディタ中の再生制御更新を行います。
		void OnEditorTick(float deltaTime) override;

		/// @brief コンポーネントのstableNameを返します。
		[[nodiscard]] std::string_view GetStableName() const override;
		/// @brief コンポーネント表示名を返します。
		[[nodiscard]] std::string_view GetComponentName() const override;
		/// @brief コンポーネントアイコンを返します。
		[[nodiscard]] uint32_t GetIcon() const override;

#ifdef _DEBUG
		/// @brief インスペクタUIを描画します。
		void DrawInspectorImGui() override;
#endif

		/// @brief JSONから設定を読み込みます。
		void Deserialize(const JsonReader& reader) override;
		/// @brief 設定をJSONへ書き込みます。
		void Serialize(JsonWriter& writer) const override;

	private:
		/// @brief ロック対象仕様です。
		struct LockTargetSpec final {
			uint64_t    componentGuid       = 0;
			uint64_t    entityGuid          = 0;
			std::string componentStableName = "";
		};

		/// @brief ロック復帰情報です。
		struct ActiveLockState final {
			BaseComponent* component      = nullptr;
			bool           previousActive = true;
		};

		/// @brief SequencePlayerを必要時に生成します。
		void EnsurePlayer();
		/// @brief 現在設定で再生開始を試みます。
		bool StartPlayback();
		/// @brief 再生停止とロック復帰を行います。
		void StopPlayback(bool restoreState);
		/// @brief 1フレーム分の再生管理更新を行います。
		void TickDirector();
		/// @brief ロック対象を無効化します。
		void ApplyLockTargets();
		/// @brief ロック対象を元状態へ戻します。
		void RestoreLockTargets();
		/// @brief ロック対象仕様からコンポーネントを解決します。
		[[nodiscard]] BaseComponent* ResolveLockTarget(const LockTargetSpec& spec) const;

		std::string mSequencePath = "";
		bool        mPlayOnAttach = true;
		bool        mAutoStopWhenCompleted = true;
		float       mPlayRate = 1.0f;
		bool        mLoop = false;
		SEQUENCE_COMPLETION_MODE mCompletionMode = SEQUENCE_COMPLETION_MODE::RESTORE_STATE;
		bool        mApplyComponentLocks = true;
		std::vector<LockTargetSpec> mLockTargets = {};

		bool        mPlayRequested = false;
		bool        mWasEvaluating = false;
		bool        mLoggedLoadFailure = false;
		uint64_t    mSequenceAssetId = 0;
		std::vector<ActiveLockState> mActiveLocks = {};

		std::shared_ptr<SequencePlayer> mPlayer = nullptr;
	};
}
