#include "TitleFlowComponent.h"

#include <algorithm>
#include <cmath>

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/ImGui/Icons.h"
#include "engine/game/GamePathResolver.h"
#include "engine/game/IGameModule.h"
#include "engine/gui/UiRoot.h"
#include "engine/gui/UiWidget.h"
#include "engine/gui/components/UiButtonBehaviorComponent.h"
#include "engine/gui/components/UiTextureComponent.h"
#include "engine/unnamed/framework/components/ui/UiCanvasComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/World.h"
#include "engine/game/IDemoService.h"
#include "game/parkour/runtime/ParkourFlowRuntimeState.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel = "TitleFlow";
		constexpr const char*      kDemoMismatchPolicyCvar =
			"demo_mismatch_policy";
		constexpr float            kDemoRetryIntervalSec = 0.75f;
		constexpr float            kMinDurationSec       = 0.01f;

		[[nodiscard]] bool IsEngineRootRelativePath(const std::string_view path) {
			return path.rfind("content/", 0) == 0 ||
			       path.rfind("projects/", 0) == 0;
		}

		[[nodiscard]] std::string ResolveTitleContentPath(
			const std::string_view configuredPath,
			const std::string_view fallbackRelativePath
		) {
			std::string effectivePath = configuredPath.empty() ?
				                            std::string(fallbackRelativePath) :
				                            std::string(configuredPath);
			if (effectivePath.empty()) {
				return {};
			}

			// 既存JSONの "content/..." 形式はプロジェクトルート基準として扱います。
			if (IsEngineRootRelativePath(effectivePath)) {
				return "./" + effectivePath;
			}

			if (const IGameModule* gameModule = ServiceLocator::Get<IGameModule>()) {
				return ResolveGameContentPath(
					gameModule->GetGameModulePaths(),
					effectivePath
				);
			}
			return effectivePath;
		}

		float EvaluateFadeEase(const float t) {
			return Math::CubicBezier(
				std::clamp(t, 0.0f, 1.0f),
				0.2f,
				0.0f,
				0.0f,
				1.0f
			);
		}
	}

	void TitleFlowComponent::OnTick(const float deltaTime) {
		TickTitle(deltaTime);
	}

	void TitleFlowComponent::OnEditorTick(const float deltaTime) {
		TickTitle(deltaTime);
	}

	void TitleFlowComponent::OnDetached() {
		ExitTitleMode();
		BaseComponent::OnDetached();
	}

	std::string_view TitleFlowComponent::GetStableName() const {
		return "parkour.TitleFlow";
	}

	std::string_view TitleFlowComponent::GetComponentName() const {
		return "TitleFlow";
	}

	uint32_t TitleFlowComponent::GetIcon() const {
		return kIconDesktopLandscape;
	}

	void TitleFlowComponent::Deserialize(const JsonReader& reader) {
		mGameplayScenePath =
			reader["gameplayScenePath"].GetString(mGameplayScenePath);
		mDemoPath               = reader["demoPath"].GetString(mDemoPath);
		mPlayPromptName         = reader["playPromptName"].GetString(mPlayPromptName);
		mFadeOverlayName        = reader["fadeOverlayName"].GetString(mFadeOverlayName);
		mFadeOutSeconds         = std::max(
			kMinDurationSec,
			reader["fadeOutSeconds"].GetFloat(mFadeOutSeconds)
		);
		mFadeInSeconds          = std::max(
			kMinDurationSec,
			reader["fadeInSeconds"].GetFloat(mFadeInSeconds)
		);
		mStartTransitionSeconds = std::max(
			kMinDurationSec,
			reader["startTransitionSeconds"].GetFloat(mStartTransitionSeconds)
		);

		// 互換性維持のため旧キーも受け取ります。
		mPlayButtonName = reader["playButtonName"].GetString(mPlayButtonName);
		mQuitButtonName = reader["quitButtonName"].GetString(mQuitButtonName);
	}

	void TitleFlowComponent::Serialize(JsonWriter& writer) const {
		writer.Key("gameplayScenePath");
		writer.Write(mGameplayScenePath);
		writer.Key("demoPath");
		writer.Write(mDemoPath);
		writer.Key("playPromptName");
		writer.Write(mPlayPromptName);
		writer.Key("fadeOverlayName");
		writer.Write(mFadeOverlayName);
		writer.Key("fadeOutSeconds");
		writer.Write(mFadeOutSeconds);
		writer.Key("fadeInSeconds");
		writer.Write(mFadeInSeconds);
		writer.Key("startTransitionSeconds");
		writer.Write(mStartTransitionSeconds);
	}

	void TitleFlowComponent::TickTitle(const float deltaTime) {
		if (!mInitialized) {
			EnterTitleMode();
		}

		ResolveUiBindings();
		ApplyUiVisibility();

		if (mQuitRequested) {
			if (ConsoleSystem* console = GetConsoleSystem()) {
				console->ExecuteCommand("quit");
			}
			mQuitRequested = false;
		}

		if (!mPlayRequested) {
			if (const InputSystem* input = GetInputSystem()) {
				if (input->IsPressed("jump")) {
					mPlayRequested = true;
				}
			}
		}
		if (mPlayRequested && mPhase != PHASE::START_TRANSITION_OUT) {
			mPhase               = PHASE::START_TRANSITION_OUT;
			mPhaseElapsedSeconds = 0.0f;
		}

		UpdatePhase(std::max(deltaTime, 0.0f));
		UpdatePlayPromptBlink(std::max(deltaTime, 0.0f));
	}

	void TitleFlowComponent::EnterTitleMode() {
		mInitialized             = true;
		mPlayRequested           = false;
		mQuitRequested           = false;
		mDemoRetrySeconds        = 0.0f;
		mHasSavedMismatchPolicy  = false;
		mSavedMismatchPolicy.clear();
		mPhase                   = PHASE::IDLE;
		mPhaseElapsedSeconds     = 0.0f;
		mPromptBlinkSeconds      = 0.0f;

		if (Entity* owner = GetOwner()) {
			owner->SetVisible(true);
		}
		if (ConsoleSystem* console = GetConsoleSystem()) {
			// タイトル用demoは演出目的なので、通常プレイ用の厳密な検証ログを抑止します。
			mSavedMismatchPolicy =
				console->GetConVarValueString(kDemoMismatchPolicyCvar);
			mHasSavedMismatchPolicy = !mSavedMismatchPolicy.empty();
			console->ExecuteCommand("demo_mismatch_policy ignore");
		}

		ResolveUiBindings();
		(void)RequestDemoPlayback();
		SetFadeOverlayAlpha(0.0f);
	}

	void TitleFlowComponent::ExitTitleMode() {
		mInitialized         = false;
		mPlayRequested       = false;
		mQuitRequested       = false;
		mDemoRetrySeconds    = 0.0f;
		mPhase               = PHASE::INACTIVE;
		mPhaseElapsedSeconds = 0.0f;
		mPromptBlinkSeconds  = 0.0f;
		mActiveDemoPath.clear();

		if (IDemoService* demo = GetDemoService()) {
			if (demo->IsPlayback() || demo->IsRecording()) {
				(void)demo->Stop();
			}
		}
		if (mHasSavedMismatchPolicy) {
			if (ConsoleSystem* console = GetConsoleSystem()) {
				console->ExecuteCommand(
					"demo_mismatch_policy " + mSavedMismatchPolicy
				);
			}
		}
		mHasSavedMismatchPolicy = false;
		mSavedMismatchPolicy.clear();

		if (mUiCanvas) {
			mUiCanvas->SetReceiveInput(false);
		}
	}

	bool TitleFlowComponent::RequestDemoPlayback() {
		IDemoService* demo = GetDemoService();
		if (!demo) {
			return false;
		}

		const std::string demoPath = ResolveDemoPath();
		if (demoPath.empty()) {
			Warning(kChannel, "Title demo path is empty.");
			mDemoRetrySeconds = kDemoRetryIntervalSec;
			return false;
		}

		// 同じdemoが再生中かつ未完了なら再起動せず、そのまま維持します。
		if (demo->IsPlayback() &&
		    !demo->IsPlaybackFinished() &&
		    demo->GetCurrentPath() == demoPath) {
			return true;
		}

		// StartPlaybackの成否を直接参照し、失敗時の毎フレーム再実行を防ぎます。
		if (!demo->StartPlayback(demoPath)) {
			Warning(
				kChannel,
				"Failed to start title demo playback. path='{}'",
				demoPath
			);
			mDemoRetrySeconds = kDemoRetryIntervalSec;
			return false;
		}

		mDemoRetrySeconds = 0.0f;
		mActiveDemoPath   = demoPath;
		return true;
	}

	void TitleFlowComponent::UpdatePhase(const float deltaTime) {
		IDemoService* demo = GetDemoService();

		// 再生開始に失敗した場合は短い間隔で再試行します。
		mDemoRetrySeconds = std::max(0.0f, mDemoRetrySeconds - deltaTime);

		// タイトル待機中のみ、停止状態ならクールダウン後に再試行します。
		if (mPhase == PHASE::IDLE &&
		    demo &&
		    !demo->IsPlayback() &&
		    !demo->IsRecording() &&
		    mDemoRetrySeconds <= 0.0f) {
			(void)RequestDemoPlayback();
		}

		switch (mPhase) {
			case PHASE::IDLE: {
				SetFadeOverlayAlpha(0.0f);
				if (demo && demo->IsPlaybackFinished()) {
					mPhase               = PHASE::DEMO_LOOP_FADE_OUT;
					mPhaseElapsedSeconds = 0.0f;
				}
				break;
			}
			case PHASE::DEMO_LOOP_FADE_OUT: {
				mPhaseElapsedSeconds += deltaTime;
				const float t = std::clamp(
					mPhaseElapsedSeconds / std::max(kMinDurationSec, mFadeOutSeconds),
					0.0f,
					1.0f
				);
				SetFadeOverlayAlpha(EvaluateFadeEase(t));
				if (t >= 1.0f) {
					if (RequestDemoPlayback()) {
						mPhase               = PHASE::DEMO_LOOP_FADE_IN;
						mPhaseElapsedSeconds = 0.0f;
					} else {
						mPhase               = PHASE::IDLE;
						mPhaseElapsedSeconds = 0.0f;
						SetFadeOverlayAlpha(0.0f);
					}
				}
				break;
			}
			case PHASE::DEMO_LOOP_FADE_IN: {
				mPhaseElapsedSeconds += deltaTime;
				const float t = std::clamp(
					mPhaseElapsedSeconds / std::max(kMinDurationSec, mFadeInSeconds),
					0.0f,
					1.0f
				);
				SetFadeOverlayAlpha(1.0f - EvaluateFadeEase(t));
				if (t >= 1.0f) {
					mPhase               = PHASE::IDLE;
					mPhaseElapsedSeconds = 0.0f;
					SetFadeOverlayAlpha(0.0f);
				}
				break;
			}
			case PHASE::START_TRANSITION_OUT: {
				mPhaseElapsedSeconds += deltaTime;
				const float t = std::clamp(
					mPhaseElapsedSeconds /
						std::max(kMinDurationSec, mStartTransitionSeconds),
					0.0f,
					1.0f
				);
				SetFadeOverlayAlpha(EvaluateFadeEase(t));
				if (t >= 1.0f) {
					CommitStartTransition();
				}
				break;
			}
			case PHASE::INACTIVE:
			default: break;
		}
	}

	void TitleFlowComponent::CommitStartTransition() {
		IDemoService* demo  = GetDemoService();
		World*       world = GetWorld();
		if (!world) {
			return;
		}

		// gameシーン側の開始カットシーンを1回だけ有効化します。
		ParkourFlowRuntimeState::MarkPendingGameStartCutscene();

		// シーン遷移前に再生を同期停止し、次シーンへの状態持ち越しを防ぎます。
		if (demo && (demo->IsPlayback() || demo->IsRecording())) {
			(void)demo->Stop();
		}

		const std::string gameplayScenePath = ResolveTitleContentPath(
			mGameplayScenePath,
			"scenes/game.json"
		);
		if (gameplayScenePath.empty()) {
			Warning(kChannel, "Gameplay scene path is empty.");
			return;
		}
		world->RequestSceneTransition(gameplayScenePath);
	}

	void TitleFlowComponent::ResolveUiBindings() {
		mUiCanvas           = nullptr;
		mPlayButtonWidget   = nullptr;
		mQuitButtonWidget   = nullptr;
		mPlayPromptTexture  = nullptr;
		mFadeOverlayTexture = nullptr;

		Entity* owner = GetOwner();
		if (!owner) {
			return;
		}

		mUiCanvas = owner->GetComponent<UiCanvasComponent>();
		if (!mUiCanvas || !mUiCanvas->EnsureRuntimeLoaded()) {
			return;
		}

		Gui::UiRoot* root             = mUiCanvas->GetRuntimeRoot();
		Gui::UiWidget* rootWidget     = root ? root->GetRootWidget() : nullptr;
		if (!rootWidget) {
			return;
		}

		mPlayButtonWidget =
			FindWidgetByNameRecursive(rootWidget, mPlayButtonName);
		mQuitButtonWidget =
			FindWidgetByNameRecursive(rootWidget, mQuitButtonName);
		Gui::UiWidget* playPromptWidget =
			FindWidgetByNameRecursive(rootWidget, mPlayPromptName);
		Gui::UiWidget* fadeOverlayWidget =
			FindWidgetByNameRecursive(rootWidget, mFadeOverlayName);

		mPlayPromptTexture =
			playPromptWidget ?
				playPromptWidget->GetOrAddComponent<Gui::UiTextureComponent>() :
				nullptr;
		mFadeOverlayTexture =
			fadeOverlayWidget ?
				fadeOverlayWidget->GetOrAddComponent<Gui::UiTextureComponent>() :
				nullptr;

		if (mPlayButtonWidget) {
			if (auto* playBehavior =
				mPlayButtonWidget->GetComponent<Gui::UiButtonBehaviorComponent>()) {
				playBehavior->SetOnClick([this]() { RequestStart(); });
			}
		}
		if (mQuitButtonWidget) {
			if (auto* quitBehavior =
				mQuitButtonWidget->GetComponent<Gui::UiButtonBehaviorComponent>()) {
				quitBehavior->SetOnClick([this]() { RequestQuit(); });
			}
		}
	}

	Gui::UiWidget* TitleFlowComponent::FindWidgetByNameRecursive(
		Gui::UiWidget*        root,
		const std::string_view widgetName
	) {
		if (!root) {
			return nullptr;
		}
		if (root->GetName() == widgetName) {
			return root;
		}

		for (const auto& child : root->GetChildren()) {
			if (!child) {
				continue;
			}
			if (Gui::UiWidget* found =
				FindWidgetByNameRecursive(child.get(), widgetName)) {
				return found;
			}
		}
		for (Gui::UiWidget* child : root->GetReferenceChildren()) {
			if (Gui::UiWidget* found =
				FindWidgetByNameRecursive(child, widgetName)) {
				return found;
			}
		}
		return nullptr;
	}

	void TitleFlowComponent::ApplyUiVisibility() const {
		if (Entity* owner = GetOwner()) {
			owner->SetVisible(mInitialized);
		}
		if (mUiCanvas) {
			mUiCanvas->SetReceiveInput(mInitialized);
		}
	}

	void TitleFlowComponent::SetFadeOverlayAlpha(const float alpha) const {
		if (!mFadeOverlayTexture) {
			return;
		}
		Gui::Color color = mFadeOverlayTexture->GetColor();
		color.r          = 0.0f;
		color.g          = 0.0f;
		color.b          = 0.0f;
		color.a          = std::clamp(alpha, 0.0f, 1.0f);
		mFadeOverlayTexture->SetColor(color);
	}

	void TitleFlowComponent::UpdatePlayPromptBlink(const float deltaTime) {
		if (!mPlayPromptTexture) {
			return;
		}

		// Idle中のみ点滅させて、遷移時は視認性を優先して固定表示にします。
		if (mPhase != PHASE::IDLE) {
			Gui::Color fixedColor = mPlayPromptTexture->GetColor();
			fixedColor.a          = 1.0f;
			mPlayPromptTexture->SetColor(fixedColor);
			return;
		}

		mPromptBlinkSeconds += deltaTime;

		const float blink = 0.5f + 0.5f * std::sin(mPromptBlinkSeconds * 4.0f);
		Gui::Color  color = mPlayPromptTexture->GetColor();
		color.a           = 0.45f + 0.55f * blink;
		mPlayPromptTexture->SetColor(color);
	}

	std::string TitleFlowComponent::ResolveDemoPath() const {
		return ResolveTitleContentPath(mDemoPath, "replay/title_demo.udemo");
	}

	void TitleFlowComponent::RequestStart() {
		mPlayRequested = true;
	}

	void TitleFlowComponent::RequestQuit() {
		mQuitRequested = true;
	}

	REGISTER_COMPONENT(TitleFlowComponent);
}
