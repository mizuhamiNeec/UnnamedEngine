#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	class AudioSourceComponent;
	class CourseProgressComponent;
	class Entity;
	class JsonReader;
	class JsonWriter;
	class UiCanvasComponent;

	namespace Gui {
		class UiDigitStripComponent;
		class UiTextureComponent;
		class UiWidget;
	}

	/// @brief コースクリア後のリザルト表示とタイトル復帰を制御するコンポーネントです。
	class CourseResultFlowComponent final : public BaseComponent {
	public:
		/// @brief コンポーネント追加時にリザルトUIを非表示へ初期化します。
		void OnAttached() override;
		/// @brief コースクリア検出、リザルト表示、タイトル遷移を更新します。
		void OnTick(float deltaTime) override;
		/// @brief エディター中は参照解決だけを行い、遷移は実行しません。
		void OnEditorTick(float deltaTime) override;
		/// @brief コンポーネント破棄時にロック解除とUI非表示を保証します。
		void OnDetached() override;

		/// @brief stableName を返します。
		[[nodiscard]] std::string_view GetStableName() const override;
		/// @brief 表示用のコンポーネント名を返します。
		[[nodiscard]] std::string_view GetComponentName() const override;
		[[nodiscard]] uint32_t         GetIcon() const override;

#ifdef _DEBUG
		/// @brief インスペクタ用UIを描画します。
		void DrawInspectorImGui() override;
#endif

		/// @brief JSON から設定を読み込みます。
		void Deserialize(const JsonReader& reader) override;
		/// @brief 設定を JSON に書き出します。
		void Serialize(JsonWriter& writer) const override;

	private:
		/// @brief リザルト遷移フェーズです。
		enum class PHASE : uint8_t {
			WAITING = 0,
			SHOW_RESULT,
			FADE_OUT,
			TRANSITIONED,
		};

		/// @brief ロック対象定義です。
		struct LockTargetSpec {
			uint64_t    entityGuid = 0;
			std::string componentStableName;
		};

		/// @brief 実行中のロック復帰情報です。
		struct ActiveLockState {
			BaseComponent* component      = nullptr;
			bool           previousActive = true;
		};

		/// @brief リザルト演出を開始します。
		void BeginResult();
		/// @brief リザルト表示フェーズを更新します。
		void TickShowResult(float deltaTime);
		/// @brief フェードアウトフェーズを更新します。
		void TickFadeOut(float deltaTime);
		/// @brief タイトルシーンへの遷移を要求します。
		void CommitTitleTransition();

		/// @brief CourseProgress/UI/Audio の参照を再解決します。
		void ResolveBindings();
		/// @brief 解決済み参照のキャッシュを初期化します。
		void ClearResolvedBindings();
		/// @brief リザルトUIの表示内容を更新します。
		void UpdateResultWidgets(float alpha) const;
		/// @brief フェードオーバーレイのアルファを更新します。
		void SetFadeOverlayAlpha(float alpha) const;
		/// @brief リザルト関連UIを非表示にします。
		void HideResultWidgets() const;

		/// @brief lockTargets を無効化して操作を抑止します。
		void ApplyLockTargets();
		/// @brief lockTargets の有効状態を復帰します。
		void RestoreLockTargets();
		/// @brief lock spec から対象コンポーネントを解決します。
		[[nodiscard]] BaseComponent* ResolveLockTarget(const LockTargetSpec& spec)
		const;
		/// @brief GUID から AudioSource を解決します。
		[[nodiscard]] AudioSourceComponent* ResolveAudioSourceByGuid(
			uint64_t componentGuid
		) const;

		/// @brief 名前でウィジェットを再帰検索します。
		[[nodiscard]] static Gui::UiWidget* FindWidgetByNameRecursive(
			Gui::UiWidget* root,
			std::string_view widgetName
		);
		/// @brief lock spec を JSON へ書き出します。
		static void SerializeLockTarget(
			JsonWriter& writer,
			const LockTargetSpec& spec
		);
		/// @brief 演出用 cubic-bezier 補間を返します。
		[[nodiscard]] static float EvaluateEase(float t);

		std::string mCourseId       = "default";
		std::string mTitleScenePath = "scenes/title.json";

		uint64_t mHudCanvasEntityGuid = 0;
		uint64_t mClearAudioSourceGuid = 0;

		std::string mResultRootWidgetName    = "CourseResultRoot";
		std::string mClearImageWidgetName    = "CourseResultClearImage";
		std::string mElapsedDigitsWidgetName = "CourseResultElapsedDigits";
		std::string mFadeOverlayWidgetName   = "OpeningFadeOverlay";
		std::string mClearTexturePath        = "textures/clear.png";
		std::string mDigitTexturePath        = "textures/digits.png";

		float mResultHoldSeconds = 2.0f;
		float mFadeOutSeconds    = 0.45f;
		int   mElapsedDigitsMinDigits = 6;
		float mElapsedDisplayScale    = 100.0f;

		std::vector<LockTargetSpec>  mLockTargets = {};
		std::vector<ActiveLockState> mActiveLocks = {};

		CourseProgressComponent* mCourseProgress = nullptr;
		UiCanvasComponent*       mHudCanvas      = nullptr;
		AudioSourceComponent*    mClearAudio     = nullptr;

		Gui::UiWidget*           mResultRootWidget    = nullptr;
		Gui::UiWidget*           mClearImageWidget    = nullptr;
		Gui::UiWidget*           mElapsedDigitsWidget = nullptr;
		Gui::UiWidget*           mFadeOverlayWidget   = nullptr;
		Gui::UiTextureComponent* mClearImageTexture   = nullptr;
		Gui::UiDigitStripComponent* mElapsedDigits    = nullptr;
		Gui::UiTextureComponent* mFadeOverlayTexture  = nullptr;

		PHASE mPhase               = PHASE::WAITING;
		float mPhaseElapsedSeconds = 0.0f;
		float mLatchedElapsedSeconds = 0.0f;
		bool  mWasCourseCleared    = false;
	};
}
