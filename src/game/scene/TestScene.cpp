#include "TestScene.h"

#include <algorithm>
#include <cstdint>
#include <cmath>

#include <engine/EngineServices.h>
#include <engine/OldConsole/Console.h>
#include <engine/TextureManager/TexManager.h>
#include <runtime/core/math/Math.h>

#include <game/components/CameraRotator.h>
#include <game/components/player/MovementComponent.h>
#include <game/components/player/PlayerInputController.h>
#include <game/replay/ReplayManager.h>

#include "GameScene.h"

#include "engine/Engine.h"
#include "engine/Input/InputSystem.h"
#include "engine/Sprite/SpriteCommon.h"

namespace {
	constexpr uint32_t kDefaultPlaybackTickRate = 66;
	constexpr int      kMaxCatchUpSteps         = 8;
	constexpr float    kTitleReferenceWidth     = 1280.0f;
	constexpr float    kTitleReferenceHeight    = 720.0f;
	constexpr char     kOverlayTexturePath[]    =
		"./content/parkour/textures/title_overlay.png";
	constexpr char kTitleLogoTexturePath[] =
		"./content/parkour/textures/title_logo.png";
	constexpr char kPressStartTexturePath[] =
		"./content/parkour/textures/press_space_start.png";
	constexpr float kDemoLoopFadeOutSec = 0.22f;
	constexpr float kDemoLoopFadeInSec  = 0.24f;

	float EvaluateFadeEase(const float t) {
		return Math::CubicBezier(
			std::clamp(t, 0.0f, 1.0f), 0.2f, 0.0f, 0.0f, 1.0f
		);
	}
}

void TestScene::ResetBackgroundDemoScene() {
	if (mBackgroundScene) {
		mBackgroundScene->Shutdown();
		mBackgroundScene.reset();
	}

	mBackgroundScene = std::make_unique<GameScene>();
	mBackgroundScene->SetDemoPlaybackEnabled(true);
	mBackgroundScene->Init();

	if (!mReplayInputController) {
		mReplayInputController = std::make_shared<
			ReplayPlayerInputController>();
	}

	if (MovementComponent* movement = mBackgroundScene->
		GetMovementComponent()) {
		movement->SetInputController(mReplayInputController);
	}
}

void TestScene::Init() {
	ReplayManager& replayManager = ReplayManager::Get();
	replayManager.Initialize();

	mReplayInputController = std::make_shared<ReplayPlayerInputController>();
	ResetBackgroundDemoScene();

	ReplayManager::ReplayClip clip;
	if (!replayManager.LoadClipFromFile(
		std::string(ReplayManager::kDefaultTitleDemoPath), clip
	)) {
		clip = replayManager.BuildDefaultTitleDemoClip();
		Console::Print(
			"Title demo clip not found. Using built-in fallback clip."
		);
		Console::Print(
			"Use 'demo_record_start ./content/parkour/replay/title_demo.json 66' then 'demo_record_stop' to create one."
		);
	}

	replayManager.StartPlayback(std::move(clip), true);
	InitTitleSprites();

	mPromptBlinkTimeSec = 0.0f;
	mTickAccumulatorSec = 0.0f;
	mDemoLoopFadeElapsedSec = 0.0f;
	mDemoLoopFadeAlpha      = 0.0f;
	mDemoLoopFadePhase      = DemoLoopFadePhase::None;
	mPendingStartGame   = false;
}

void TestScene::Update(const float deltaTime) {
	if (!mBackgroundScene) { return; }

	if (!mPendingStartGame && (HandleStartInput() || HandleStartViaUiClick())) {
		mPendingStartGame = true;
		RequestSceneChange("GameScene");
		return;
	}

	const uint32_t playbackTickRate = std::max(
		1u,
		ReplayManager::Get().GetTickRateOrDefault(kDefaultPlaybackTickRate)
	);
	const float playbackTickSec = 1.0f / static_cast<float>(playbackTickRate);

	mTickAccumulatorSec += deltaTime;
	const bool blockPlaybackTicks =
		(mDemoLoopFadePhase == DemoLoopFadePhase::FadeOut);

	int stepCount = 0;
	while (!blockPlaybackTicks &&
	       mTickAccumulatorSec >= playbackTickSec &&
	       stepCount < kMaxCatchUpSteps) {
		ReplayUserCmdFrame frame;
		bool               didLoop = false;
		ReplayManager::Get().ConsumePlaybackTick(frame, &didLoop);
		if (didLoop) {
			StartDemoLoopFade();
			mTickAccumulatorSec = 0.0f;
			break;
		}

		if (mReplayInputController) {
			mReplayInputController->SetReplayInput(
				{frame.moveX, frame.moveY},
				frame.buttons
			);
		}

		if (CameraRotator* cameraRotator = mBackgroundScene->
			GetCameraRotator()) {
			cameraRotator->SetLookAnglesDegrees(
				frame.viewPitchDeg, frame.viewYawDeg
			);
		}

		mBackgroundScene->Update(playbackTickSec);
		mBackgroundScene->ApplyReplayAuthoritativeState(frame);

		mTickAccumulatorSec -= playbackTickSec;
		++stepCount;
	}

	if (stepCount >= kMaxCatchUpSteps) { mTickAccumulatorSec = 0.0f; }

	UpdateDemoLoopFade(deltaTime);
	UpdateTitleSprites(deltaTime);
}

void TestScene::StartDemoLoopFade() {
	if (mDemoLoopFadePhase != DemoLoopFadePhase::None) { return; }
	mDemoLoopFadePhase      = DemoLoopFadePhase::FadeOut;
	mDemoLoopFadeElapsedSec = 0.0f;
	mDemoLoopFadeAlpha      = 0.0f;
}

void TestScene::UpdateDemoLoopFade(const float deltaTime) {
	switch (mDemoLoopFadePhase) {
		case DemoLoopFadePhase::FadeOut: {
			mDemoLoopFadeElapsedSec += deltaTime;
			const float t = std::clamp(
				mDemoLoopFadeElapsedSec / std::max(0.01f, kDemoLoopFadeOutSec),
				0.0f,
				1.0f
			);
			mDemoLoopFadeAlpha = EvaluateFadeEase(t);
			if (t >= 1.0f) {
				ResetBackgroundDemoScene();
				mTickAccumulatorSec      = 0.0f;
				mDemoLoopFadePhase       = DemoLoopFadePhase::FadeIn;
				mDemoLoopFadeElapsedSec  = 0.0f;
				mDemoLoopFadeAlpha       = 1.0f;
			}
			break;
		}
		case DemoLoopFadePhase::FadeIn: {
			mDemoLoopFadeElapsedSec += deltaTime;
			const float t = std::clamp(
				mDemoLoopFadeElapsedSec / std::max(0.01f, kDemoLoopFadeInSec),
				0.0f,
				1.0f
			);
			mDemoLoopFadeAlpha = 1.0f - EvaluateFadeEase(t);
			if (t >= 1.0f) {
				mDemoLoopFadePhase      = DemoLoopFadePhase::None;
				mDemoLoopFadeElapsedSec = 0.0f;
				mDemoLoopFadeAlpha      = 0.0f;
			}
			break;
		}
		case DemoLoopFadePhase::None:
		default: break;
	}
}

void TestScene::Render() {
	if (mBackgroundScene) { mBackgroundScene->Render(); }

	if (mSpriteCommon) { mSpriteCommon->Render(); }
	//if (mTitleBackdropSprite) { mTitleBackdropSprite->Draw(); }
	if (mDemoLoopFadeSprite && mDemoLoopFadeAlpha > 0.0f) {
		mDemoLoopFadeSprite->Draw();
	}
	if (mTitleLogoSprite) { mTitleLogoSprite->Draw(); }
	if (mPressStartSprite) { mPressStartSprite->Draw(); }
}

void TestScene::Shutdown() {
	ReplayManager::Get().StopPlayback();

	if (mBackgroundScene) {
		mBackgroundScene->Shutdown();
		mBackgroundScene.reset();
	}

	ShutdownTitleSprites();

	mReplayInputController.reset();
	mPromptBlinkTimeSec = 0.0f;
	mTickAccumulatorSec = 0.0f;
	mDemoLoopFadeElapsedSec = 0.0f;
	mDemoLoopFadeAlpha      = 0.0f;
	mDemoLoopFadePhase      = DemoLoopFadePhase::None;
	mPendingStartGame   = false;
}

bool TestScene::HandleStartInput() const {
	return InputSystem::IsTriggered("jump");
}

bool TestScene::HandleStartViaUiClick() const {
	// TODO: UI描画実装後、ここでPlayボタンのクリック遷移に差し替える。
	return false;
}

void TestScene::InitTitleSprites() {
	auto* engine = Unnamed::EngineServices::Get();
	if (!engine) { return; }

	mSpriteCommon    = engine->GetSpriteCommonInstance();
	auto* texManager = engine->GetTexManagerInstance();
	if (!mSpriteCommon || !texManager) { return; }

	texManager->LoadTexture(kOverlayTexturePath);
	texManager->LoadTexture(kTitleLogoTexturePath);
	texManager->LoadTexture(kPressStartTexturePath);

	mTitleBackdropSprite = std::make_unique<Sprite>();
	mTitleBackdropSprite->Init(mSpriteCommon, kOverlayTexturePath);
	mTitleBackdropSprite->SetAnchorPoint({0.0f, 0.0f});
	mTitleBackdropSprite->SetColor({0.0f, 0.0f, 0.0f, 0.35f});

	mDemoLoopFadeSprite = std::make_unique<Sprite>();
	mDemoLoopFadeSprite->Init(mSpriteCommon, kOverlayTexturePath);
	mDemoLoopFadeSprite->SetAnchorPoint({0.0f, 0.0f});
	mDemoLoopFadeSprite->SetColor({0.0f, 0.0f, 0.0f, 0.0f});

	mTitleLogoSprite = std::make_unique<Sprite>();
	mTitleLogoSprite->Init(mSpriteCommon, kTitleLogoTexturePath);
	mTitleLogoSprite->SetAnchorPoint({0.5f, 0.5f});
	mTitleLogoSprite->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
	mTitleLogoBaseSize = mTitleLogoSprite->GetSize();

	mPressStartSprite = std::make_unique<Sprite>();
	mPressStartSprite->Init(mSpriteCommon, kPressStartTexturePath);
	mPressStartSprite->SetAnchorPoint({0.5f, 0.5f});
	mPressStartSprite->SetColor({1.0f, 1.0f, 1.0f, 0.9f});
	mPressStartBaseSize = mPressStartSprite->GetSize();

	UpdateTitleSpriteLayout();
	UpdateTitleSprites(0.0f);
}

void TestScene::UpdateTitleSprites(const float deltaTime) {
	UpdateTitleSpriteLayout();

	mPromptBlinkTimeSec += deltaTime;
	const float blink   = 0.5f + 0.5f * std::sin(mPromptBlinkTimeSec * 4.0f);
	if (mPressStartSprite) {
		mPressStartSprite->SetColor({1.0f, 1.0f, 1.0f, 0.45f + 0.55f * blink});
	}

	if (mDemoLoopFadeSprite) {
		mDemoLoopFadeSprite->SetColor({0.0f, 0.0f, 0.0f, mDemoLoopFadeAlpha});
	}

	if (mTitleBackdropSprite) { mTitleBackdropSprite->Update(); }
	if (mDemoLoopFadeSprite) { mDemoLoopFadeSprite->Update(); }
	if (mTitleLogoSprite) { mTitleLogoSprite->Update(); }
	if (mPressStartSprite) { mPressStartSprite->Update(); }
}

void TestScene::UpdateTitleSpriteLayout() {
	const Vec2 viewportSize = Unnamed::EngineServices::Get() ?
		                          Unnamed::EngineServices::Get()->
		                          GetViewportSizeInstance() :
		                          Vec2(1280.0f, 720.0f);
	const float viewW           = std::max(1.0f, viewportSize.x);
	const float viewH           = std::max(1.0f, viewportSize.y);
	const float referenceAspect = kTitleReferenceWidth / kTitleReferenceHeight;
	const float viewportAspect  = viewW / viewH;

	float layoutW = viewW;
	float layoutH = viewH;
	if (viewportAspect > referenceAspect) {
		layoutW = viewH * referenceAspect;
	} else { layoutH = viewW / referenceAspect; }

	const float layoutLeft    = (viewW - layoutW) * 0.5f;
	const float layoutTop     = (viewH - layoutH) * 0.5f;
	const float layoutBottom  = layoutTop + layoutH;
	const float centerX       = layoutLeft + layoutW * 0.5f;
	const float titleTopY     = layoutTop + layoutH * 0.24f;
	const float promptBottomY = layoutBottom - layoutH * 0.13f;

	if (mTitleBackdropSprite) {
		mTitleBackdropSprite->SetPos({0.0f, 0.0f, 90.0f});
		mTitleBackdropSprite->SetSize({viewW, viewH, 1.0f});
	}
	if (mDemoLoopFadeSprite) {
		mDemoLoopFadeSprite->SetPos({0.0f, 0.0f, 90.5f});
		mDemoLoopFadeSprite->SetSize({viewW, viewH, 1.0f});
	}

	if (mTitleLogoSprite) {
		const float targetWidth = std::min(layoutW * 0.72f, viewW * 0.82f);
		const float baseWidth = std::max(1.0f, mTitleLogoBaseSize.x);
		const float scale = std::clamp(targetWidth / baseWidth, 0.35f, 1.35f);
		mTitleLogoSprite->SetPos({centerX, titleTopY, 91.0f});
		mTitleLogoSprite->SetSize(
			{
				mTitleLogoBaseSize.x * scale,
				mTitleLogoBaseSize.y * scale,
				1.0f
			}
		);
	}

	if (mPressStartSprite) {
		const float targetWidth = std::min(layoutW * 0.40f, viewW * 0.48f);
		const float baseWidth = std::max(1.0f, mPressStartBaseSize.x);
		const float scale = std::clamp(targetWidth / baseWidth, 0.45f, 1.2f);
		const float promptHalfHeight = mPressStartBaseSize.y * scale * 0.5f;
		const float promptMinY = layoutTop + layoutH * 0.55f;
		const float promptMaxY = layoutBottom - promptHalfHeight;
		const float promptY = (promptMinY <= promptMaxY) ?
			                      std::clamp(
				                      promptBottomY - promptHalfHeight,
				                      promptMinY,
				                      promptMaxY
			                      ) :
			                      layoutTop + layoutH * 0.72f;
		mPressStartSprite->SetPos({centerX, promptY, 91.0f});
		mPressStartSprite->SetSize(
			{
				mPressStartBaseSize.x * scale,
				mPressStartBaseSize.y * scale,
				1.0f
			}
		);
	}
}

void TestScene::ShutdownTitleSprites() {
	mPressStartSprite.reset();
	mTitleLogoSprite.reset();
	mDemoLoopFadeSprite.reset();
	mTitleBackdropSprite.reset();
	mSpriteCommon = nullptr;
}
