#include <engine/SceneManager/SceneManager.h>

#include <algorithm>
#include <cmath>

#include <engine/Camera/CameraManager.h>
#include <engine/Engine.h>
#include <engine/EngineServices.h>
#include <engine/Sprite/SpriteCommon.h>
#include <engine/TextureManager/TexManager.h>
#include <engine/unnamed/subsystem/console/Log.h>
#include <runtime/core/math/Math.h>

namespace {
	constexpr char  kTransitionTexturePath[]    = "./content/parkour/textures/title_overlay.png";
	constexpr float kTransitionPhaseDurationSec = 2.0f;
	constexpr float kTransitionBackdropMaxAlpha = 1.0f;
	constexpr float kTransitionPanelRotationRad = 12.0f * Math::deg2Rad;
	constexpr std::array<float, 4> kTransitionPanelWidthFactors = {
		0.22f, 0.30f, 0.16f, 0.07f
	};
	constexpr std::array<float, 4> kTransitionPanelStaggers = {
		0.00f, 0.06f, 0.14f, 0.22f
	};
	constexpr std::array<float, 4> kTransitionPanelBrightness = {
		0.08f, 0.14f, 0.22f, 0.78f
	};

	float Saturate(const float value) {
		return std::clamp(value, 0.0f, 1.0f);
	}

	float EvaluateTransitionEase(const float t) {
		return Math::CubicBezier(Saturate(t), 0.20f, 0.00f, 0.32f, 1.00f);
	}
}

/// @brief コンストラクタ
/// @param factory シーンファクトリーへの参照
SceneManager::SceneManager(SceneFactory& factory) : mFactory(factory) {}

/// @brief シーンを変更します（即時実行）
/// @param name シーン名
void SceneManager::ChangeScene(const std::string& name) {
	std::shared_ptr<BaseScene> newScene = mFactory.CreateScene(name);
	if (!newScene) {
		Error("SceneManager", "Failed to create scene: {}", name);
		return;
	}

	// 旧シーンのシャットダウン
	if (mCurrentScene) {
		Msg("SceneManager", "Shutting down scene: {}", mCurrentSceneName);
		mCurrentScene->Shutdown();
		mCurrentScene.reset();
	}

	// シーン遷移時にカメラをクリア
	CameraManager::Clear();

	// 新シーンの初期化
	mCurrentScene     = newScene;
	mCurrentSceneName = name;
	mCurrentScene->Init();

	Msg("SceneManager", "Scene changed to: {}", name);
}

/// @brief シーン遷移をリクエストします（遅延実行）
/// @param name 遷移先のシーン名
void SceneManager::RequestSceneChange(const std::string& name) {
	if (name.empty()) { return; }

	if (name != mCurrentSceneName) { mPendingCurrentSceneReload = false; }

	if (name == mCurrentSceneName && !IsTransitionActive() &&
	    !mPendingTransitionSwap && !mPendingCurrentSceneReload) {
		DevMsg(
			"SceneManager",
			"Ignored scene change request to current scene: {}",
			name
		);
		return;
	}

	if (mPendingTransitionSwap) {
		DevMsg(
			"SceneManager",
			"Queued scene change request during pending swap: {}",
			name
		);
		mPendingSceneName = name;
		return;
	}

	if (IsTransitionActive()) {
		DevMsg(
			"SceneManager",
			"Queued scene change request during active transition: {}",
			name
		);
		mPendingSceneName = name;
		return;
	}

	mPendingSceneName = name;
	Msg("SceneManager", "Scene change requested: {}", name);
}

void SceneManager::RequestCurrentSceneReload() {
	if (!mCurrentScene) { return; }

	mPendingSceneName          = mCurrentSceneName;
	mPendingCurrentSceneReload = true;
	Msg("SceneManager", "Current scene reload requested: {}", mCurrentSceneName);
}

/// @brief ペンディング中のシーン遷移を処理します
void SceneManager::ProcessPendingSceneChange() {
	if (mPendingTransitionSwap && mTransitionTargetSceneName.has_value()) {
		const std::string sceneName = *mTransitionTargetSceneName;
		mPendingTransitionSwap      = false;
		mTransitionTargetSceneName.reset();

		ChangeScene(sceneName);
		mTransitionPhase      = TransitionPhase::Enter;
		mTransitionElapsedSec = 0.0f;
		return;
	}

	if (IsTransitionActive() || !mPendingSceneName.has_value()) { return; }

	std::string sceneName = *mPendingSceneName;
	mPendingSceneName.reset();

	const bool reloadCurrentScene =
		mPendingCurrentSceneReload && sceneName == mCurrentSceneName;
	if (sceneName == mCurrentSceneName && !reloadCurrentScene) { return; }
	mPendingCurrentSceneReload = false;

	if (!mCurrentScene) {
		ChangeScene(sceneName);
		return;
	}

	BeginSceneTransition(sceneName);
}

void SceneManager::ShutdownCurrentScene() {
	mPendingSceneName.reset();
	mTransitionTargetSceneName.reset();
	mPendingTransitionSwap   = false;
	mPendingCurrentSceneReload = false;
	mTransitionPhase         = TransitionPhase::None;
	mTransitionElapsedSec    = 0.0f;
	mTransitionCoverProgress = 0.0f;

	if (mCurrentScene) {
		Msg("SceneManager", "Shutting down scene: {}", mCurrentSceneName);
		mCurrentScene->Shutdown();
		mCurrentScene.reset();
	}

	mCurrentSceneName.clear();
	CameraManager::Clear();
}

/// @brief シーンを更新します
/// @param deltaTime 前フレームからの経過時間（秒）
void SceneManager::Update(const float deltaTime) {
	if (mTransitionPhase == TransitionPhase::Exit) {
		mTransitionElapsedSec += deltaTime;
		const float normalized =
			Saturate(mTransitionElapsedSec / kTransitionPhaseDurationSec);
		mTransitionCoverProgress = EvaluateTransitionEase(normalized);
		UpdateTransitionOverlay();

		if (normalized >= 1.0f) {
			mTransitionCoverProgress = 1.0f;
			mPendingTransitionSwap   = true;
		}
		return;
	}

	if (mCurrentScene) { mCurrentScene->Update(deltaTime); }

	if (mTransitionPhase == TransitionPhase::Enter) {
		mTransitionElapsedSec += deltaTime;
		const float normalized =
			Saturate(mTransitionElapsedSec / kTransitionPhaseDurationSec);
		mTransitionCoverProgress = 1.0f - EvaluateTransitionEase(normalized);
		UpdateTransitionOverlay();

		if (normalized >= 1.0f) {
			mTransitionPhase         = TransitionPhase::None;
			mTransitionElapsedSec    = 0.0f;
			mTransitionCoverProgress = 0.0f;
			UpdateTransitionOverlay();
		}
	}
}

/// @brief シーンをレンダリングします
void SceneManager::Render() const {
	if (mCurrentScene) { mCurrentScene->Render(); }
	DrawTransitionOverlay();
}

/// @brief 現在のシーンを取得します
std::shared_ptr<BaseScene> SceneManager::GetCurrentScene() const {
	return mCurrentScene;
}

void SceneManager::EnsureTransitionSprites() {
	if (mTransitionBackdropSprite) { return; }

	auto* engine = Unnamed::EngineServices::Get();
	if (!engine) { return; }

	SpriteCommon* spriteCommon = engine->GetSpriteCommonInstance();
	TexManager*   texManager   = engine->GetTexManagerInstance();
	if (!spriteCommon || !texManager) { return; }

	texManager->LoadTexture(kTransitionTexturePath);

	mTransitionBackdropSprite = std::make_unique<Sprite>();
	mTransitionBackdropSprite->Init(spriteCommon, kTransitionTexturePath);
	mTransitionBackdropSprite->SetAnchorPoint({0.0f, 0.0f});

	for (auto& panelSprite : mTransitionPanelSprites) {
		panelSprite = std::make_unique<Sprite>();
		panelSprite->Init(spriteCommon, kTransitionTexturePath);
		panelSprite->SetAnchorPoint({0.5f, 0.5f});
		panelSprite->SetRot({0.0f, 0.0f, kTransitionPanelRotationRad});
	}

	UpdateTransitionOverlay();
}

void SceneManager::UpdateTransitionOverlay() {
	EnsureTransitionSprites();
	if (!mTransitionBackdropSprite) { return; }

	auto* engine = Unnamed::EngineServices::Get();
	const Vec2 viewportSize =
		engine ? engine->GetViewportSizeInstance() : Vec2(1280.0f, 720.0f);
	const float viewW = std::max(1.0f, viewportSize.x);
	const float viewH = std::max(1.0f, viewportSize.y);
	const float diagonal = std::sqrt(viewW * viewW + viewH * viewH);
	const float centerY  = viewH * 0.5f;

	mTransitionBackdropSprite->SetPos({0.0f, 0.0f, 0.04f});
	mTransitionBackdropSprite->SetSize({viewW, viewH, 1.0f});
	mTransitionBackdropSprite->SetColor({
		0.0f,
		0.0f,
		0.0f,
		kTransitionBackdropMaxAlpha * mTransitionCoverProgress
	});
	mTransitionBackdropSprite->Update();

	const float travelStartX = viewW + diagonal * 1.1f;
	const float travelEndX   = -diagonal * 1.1f;
	const float panelHeight  = diagonal * 1.35f;

	for (size_t i = 0; i < mTransitionPanelSprites.size(); ++i) {
		Sprite* panelSprite = mTransitionPanelSprites[i].get();
		if (!panelSprite) { continue; }

		const float staggeredProgress = Saturate(
			(mTransitionCoverProgress - kTransitionPanelStaggers[i]) /
			(1.0f - kTransitionPanelStaggers[i])
		);
		const float panelProgress = EvaluateTransitionEase(staggeredProgress);
		const float width         = diagonal * kTransitionPanelWidthFactors[i];
		const float baseYOffset   =
			(static_cast<float>(i) - 1.5f) * viewH * 0.06f;
		const float sweepYOffset =
			(0.5f - panelProgress) * viewH * 0.18f;
		const float x = Math::Lerp(
			travelStartX + width * (0.35f + static_cast<float>(i) * 0.08f),
			travelEndX - width * 0.25f,
			panelProgress
		);
		const float brightness = kTransitionPanelBrightness[i];
		const float alpha =
			(i == mTransitionPanelSprites.size() - 1 ? 0.88f : 0.62f) *
			mTransitionCoverProgress;

		panelSprite->SetPos({
			x,
			centerY + baseYOffset + sweepYOffset,
			0.03f - static_cast<float>(i) * 0.01f
		});
		panelSprite->SetSize({width, panelHeight, 1.0f});
		panelSprite->SetColor({brightness, brightness, brightness, alpha});
		panelSprite->Update();
	}
}

void SceneManager::DrawTransitionOverlay() const {
	if (mTransitionCoverProgress <= 0.0f) { return; }

	auto* engine = Unnamed::EngineServices::Get();
	if (!engine) { return; }

	SpriteCommon* spriteCommon = engine->GetSpriteCommonInstance();
	if (!spriteCommon || !mTransitionBackdropSprite) { return; }

	spriteCommon->Render();
	mTransitionBackdropSprite->Draw();

	for (const auto& panelSprite : mTransitionPanelSprites) {
		if (panelSprite) { panelSprite->Draw(); }
	}
}

void SceneManager::BeginSceneTransition(const std::string& name) {
	EnsureTransitionSprites();
	mTransitionTargetSceneName = name;
	mTransitionPhase           = TransitionPhase::Exit;
	mTransitionElapsedSec      = 0.0f;
	mTransitionCoverProgress   = 0.0f;
	mPendingTransitionSwap     = false;
	UpdateTransitionOverlay();

	Msg(
		"SceneManager",
		"Scene transition started: {} -> {}",
		mCurrentSceneName,
		name
	);
}

bool SceneManager::IsTransitionActive() const {
	return mTransitionPhase != TransitionPhase::None;
}
