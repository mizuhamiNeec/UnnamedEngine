#pragma once

#include <string>
#include <string_view>

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;
	class UiCanvasComponent;

	namespace Gui {
		class UiWidget;
		class UiTextureComponent;
	}

	/// @brief タイトル演出（デモ再生/フェード/開始遷移）を制御するコンポーネントです。
	class TitleFlowComponent final : public BaseComponent {
	public:
		/// @brief タイトル演出を更新します。
		/// @param deltaTime 前フレームからの経過時間（秒）
		void OnTick(float deltaTime) override;
		/// @brief エディタ中も同じタイトル演出を更新します。
		/// @param deltaTime 前フレームからの経過時間（秒）
		void OnEditorTick(float deltaTime) override;
		/// @brief コンポーネント取り外し時に状態を解除します。
		void OnDetached() override;

		/// @brief コンポーネントの stable name を返します。
		[[nodiscard]] std::string_view GetStableName() const override;
		/// @brief コンポーネントの表示名を返します。
		[[nodiscard]] std::string_view GetComponentName() const override;
		[[nodiscard]] uint32_t         GetIcon() const override;

		/// @brief JSON から設定を読み込みます。
		void Deserialize(const JsonReader& reader) override;
		/// @brief 設定を JSON へ書き出します。
		void Serialize(JsonWriter& writer) const override;

	private:
		enum class PHASE : uint8_t {
			INACTIVE = 0,
			IDLE,
			DEMO_LOOP_FADE_OUT,
			DEMO_LOOP_FADE_IN,
			START_TRANSITION_OUT,
		};

		/// @brief タイトル演出を1フレーム更新します。
		void TickTitle(float deltaTime);
		/// @brief タイトル演出開始時の初期化を行います。
		void EnterTitleMode();
		/// @brief タイトル演出終了時の後始末を行います。
		void ExitTitleMode();
		/// @brief タイトル用demo再生を開始します。
		/// @return 再生開始に成功した場合はtrue
		[[nodiscard]] bool RequestDemoPlayback();
		/// @brief タイトル演出フェーズを更新します。
		void UpdatePhase(float deltaTime);
		/// @brief ゲーム開始遷移を確定します。
		void CommitStartTransition();
		/// @brief UI参照とクリックコールバックを解決します。
		void ResolveUiBindings();
		/// @brief 名前でウィジェットを再帰検索します。
		[[nodiscard]] static Gui::UiWidget* FindWidgetByNameRecursive(
			Gui::UiWidget* root,
			std::string_view widgetName
		);
		/// @brief UI可視状態を現在のタイトル状態へ反映します。
		void ApplyUiVisibility() const;
		/// @brief フェードオーバーレイのアルファを更新します。
		void SetFadeOverlayAlpha(float alpha) const;
		/// @brief Playプロンプトの点滅を更新します。
		void UpdatePlayPromptBlink(float deltaTime);
		/// @brief タイトルdemoパスを解決します。
		[[nodiscard]] std::string ResolveDemoPath() const;
		/// @brief クリックイベントで開始要求を受け取ります。
		void RequestStart();
		/// @brief クリックイベントで終了要求を受け取ります。
		void RequestQuit();

		std::string mGameplayScenePath = "scenes/game.json";
		std::string mDemoPath          = "replay/title_demo.udemo";
		std::string mPlayButtonName    = "PlayButton";
		std::string mQuitButtonName    = "QuitButton";
		std::string mPlayPromptName    = "PlayImage";
		std::string mFadeOverlayName   = "FadeOverlayImage";
		float       mFadeOutSeconds    = 0.22f;
		float       mFadeInSeconds     = 0.24f;
		float       mStartTransitionSeconds = 0.30f;

		// 所有しない参照です。ランタイムUI再生成を考慮し毎Tick再解決します。
		UiCanvasComponent*       mUiCanvas           = nullptr;
		Gui::UiWidget*           mPlayButtonWidget   = nullptr;
		Gui::UiWidget*           mQuitButtonWidget   = nullptr;
		Gui::UiTextureComponent* mPlayPromptTexture  = nullptr;
		Gui::UiTextureComponent* mFadeOverlayTexture = nullptr;

		PHASE       mPhase               = PHASE::INACTIVE;
		float       mPhaseElapsedSeconds = 0.0f;
		float       mPromptBlinkSeconds  = 0.0f;
		bool        mInitialized         = false;
		bool        mPlayRequested       = false;
		bool        mQuitRequested       = false;
		float       mDemoRetrySeconds    = 0.0f;
		bool        mHasSavedMismatchPolicy = false;
		std::string mActiveDemoPath;
		std::string mSavedMismatchPolicy;
	};
}
