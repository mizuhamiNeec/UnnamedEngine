#include "TestScene.h"

#include <algorithm>
#include <cstdint>
#include <cmath>

#include <engine/EngineServices.h>
#include <engine/OldConsole/Console.h>
#include <engine/TextureManager/TexManager.h>

#include <game/components/CameraRotator.h>
#include <game/components/player/MovementComponent.h>
#include <game/components/player/PlayerInputController.h>
#include <game/replay/ReplayManager.h>

#include "GameScene.h"

#include "engine/Engine.h"
#include "engine/Sprite/SpriteCommon.h"

namespace {
	constexpr uint32_t kDefaultPlaybackTickRate = 66;
	constexpr int      kMaxCatchUpSteps         = 8;
	constexpr char     kOverlayTexturePath[]    = "./content/parkour/textures/title_overlay.png";
	constexpr char     kTitleLogoTexturePath[]  = "./content/parkour/textures/title_logo.png";
	constexpr char     kPressStartTexturePath[] = "./content/parkour/textures/press_space_start.png";
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
		mReplayInputController = std::make_shared<ReplayPlayerInputController>();
	}

	if (MovementComponent* movement = mBackgroundScene->GetMovementComponent()) {
		movement->SetInputController(mReplayInputController);
	}
}

void TestScene::Init() {
	ReplayManager& replayManager = ReplayManager::Get();
	replayManager.Initialize();

	mReplayInputController = std::make_shared<ReplayPlayerInputController>();
	ResetBackgroundDemoScene();

	ReplayManager::ReplayClip clip;
	if (!replayManager.LoadClipFromFile(std::string(ReplayManager::kDefaultTitleDemoPath), clip)) {
		clip = replayManager.BuildDefaultTitleDemoClip();
		Console::Print("Title demo clip not found. Using built-in fallback clip.");
		Console::Print("Use 'demo_record_start ./content/parkour/replay/title_demo.json 66' then 'demo_record_stop' to create one.");
	}

	replayManager.StartPlayback(std::move(clip), true);
	InitTitleSprites();

	mPromptBlinkTimeSec = 0.0f;
	mTickAccumulatorSec = 0.0f;
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

	int stepCount = 0;
	while (mTickAccumulatorSec >= playbackTickSec &&
	       stepCount < kMaxCatchUpSteps) {
		ReplayUserCmdFrame frame;
		bool               didLoop = false;
		ReplayManager::Get().ConsumePlaybackTick(frame, &didLoop);
		if (didLoop) { ResetBackgroundDemoScene(); }

		if (mReplayInputController) {
			mReplayInputController->SetReplayInput(
				{frame.moveX, frame.moveY},
				frame.buttons
			);
		}

		if (CameraRotator* cameraRotator = mBackgroundScene->GetCameraRotator()) {
			cameraRotator->SetLookAnglesDegrees(frame.viewPitchDeg, frame.viewYawDeg);
		}

		mBackgroundScene->Update(playbackTickSec);
		mBackgroundScene->ApplyReplayAuthoritativeState(frame);

		mTickAccumulatorSec -= playbackTickSec;
		++stepCount;
	}

	if (stepCount >= kMaxCatchUpSteps) {
		mTickAccumulatorSec = 0.0f;
	}

	UpdateTitleSprites(deltaTime);
}

void TestScene::Render() {
	if (mBackgroundScene) { mBackgroundScene->Render(); }

	if (mSpriteCommon) { mSpriteCommon->Render(); }
	//if (mTitleBackdropSprite) { mTitleBackdropSprite->Draw(); }
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
	mPendingStartGame   = false;
}

bool TestScene::HandleStartInput() const {
	return (GetAsyncKeyState(VK_SPACE) & 0x0001) != 0;
}

bool TestScene::HandleStartViaUiClick() const {
	// TODO: UI描画実装後、ここでPlayボタンのクリック遷移に差し替える。
	return false;
}

void TestScene::InitTitleSprites() {
	auto* engine = Unnamed::EngineServices::Get();
	if (!engine) { return; }

	mSpriteCommon = engine->GetSpriteCommonInstance();
	auto* texManager = engine->GetTexManagerInstance();
	if (!mSpriteCommon || !texManager) { return; }

	texManager->LoadTexture(kOverlayTexturePath);
	texManager->LoadTexture(kTitleLogoTexturePath);
	texManager->LoadTexture(kPressStartTexturePath);

	mTitleBackdropSprite = std::make_unique<Sprite>();
	mTitleBackdropSprite->Init(mSpriteCommon, kOverlayTexturePath);
	mTitleBackdropSprite->SetAnchorPoint({0.0f, 0.0f});
	mTitleBackdropSprite->SetColor({0.0f, 0.0f, 0.0f, 0.35f});

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
	const float blink = 0.5f + 0.5f * std::sin(mPromptBlinkTimeSec * 4.0f);
	if (mPressStartSprite) {
		mPressStartSprite->SetColor({1.0f, 1.0f, 1.0f, 0.45f + 0.55f * blink});
	}

	if (mTitleBackdropSprite) { mTitleBackdropSprite->Update(); }
	if (mTitleLogoSprite) { mTitleLogoSprite->Update(); }
	if (mPressStartSprite) { mPressStartSprite->Update(); }
}

void TestScene::UpdateTitleSpriteLayout() {
	const Vec2 viewportSize = Unnamed::EngineServices::Get() ?
		                          Unnamed::EngineServices::Get()->GetViewportSizeInstance() :
		                          Vec2(1280.0f, 720.0f);
	const float viewW = std::max(1.0f, viewportSize.x);
	const float viewH = std::max(1.0f, viewportSize.y);

	if (mTitleBackdropSprite) {
		mTitleBackdropSprite->SetPos({0.0f, 0.0f, 90.0f});
		mTitleBackdropSprite->SetSize({viewW, viewH, 1.0f});
	}

	if (mTitleLogoSprite) {
		const float targetWidth = viewW * 0.64f;
		const float baseWidth = std::max(1.0f, mTitleLogoBaseSize.x);
		const float scale = std::clamp(targetWidth / baseWidth, 0.25f, 1.5f);
		mTitleLogoSprite->SetPos({viewW * 0.5f, viewH * 0.28f, 91.0f});
		mTitleLogoSprite->SetSize({
			mTitleLogoBaseSize.x * scale,
			mTitleLogoBaseSize.y * scale,
			1.0f
		});
	}

	if (mPressStartSprite) {
		const float targetWidth = viewW * 0.36f;
		const float baseWidth = std::max(1.0f, mPressStartBaseSize.x);
		const float scale = std::clamp(targetWidth / baseWidth, 0.25f, 1.5f);
		mPressStartSprite->SetPos({viewW * 0.5f, viewH * 0.78f, 91.0f});
		mPressStartSprite->SetSize({
			mPressStartBaseSize.x * scale,
			mPressStartBaseSize.y * scale,
			1.0f
		});
	}
}

void TestScene::ShutdownTitleSprites() {
	mPressStartSprite.reset();
	mTitleLogoSprite.reset();
	mTitleBackdropSprite.reset();
	mSpriteCommon = nullptr;
}
