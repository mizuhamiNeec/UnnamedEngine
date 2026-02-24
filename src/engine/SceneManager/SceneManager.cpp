#include <engine/SceneManager/SceneManager.h>

#include <engine/Camera/CameraManager.h>
#include <engine/unnamed/subsystem/console/Log.h>

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
	mPendingSceneName = name;
	Msg("SceneManager", "Scene change requested: {}", name);
}

/// @brief ペンディング中のシーン遷移を処理します
void SceneManager::ProcessPendingSceneChange() {
	if (!mPendingSceneName.has_value()) { return; }

	std::string sceneName = mPendingSceneName.value();
	mPendingSceneName.reset();

	ChangeScene(sceneName);
}

void SceneManager::ShutdownCurrentScene() {
	mPendingSceneName.reset();

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
void SceneManager::Update(const float deltaTime) const {
	if (mCurrentScene) { mCurrentScene->Update(deltaTime); }
}

/// @brief シーンをレンダリングします
void SceneManager::Render() const {
	if (mCurrentScene) { mCurrentScene->Render(); }
}

/// @brief 現在のシーンを取得します
std::shared_ptr<BaseScene> SceneManager::GetCurrentScene() const {
	return mCurrentScene;
}
