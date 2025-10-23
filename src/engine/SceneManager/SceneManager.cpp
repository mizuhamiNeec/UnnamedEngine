#include <engine/SceneManager/SceneManager.h>

/// @brief コンストラクタ
/// @param factory シーンファクトリーへの参照
SceneManager::SceneManager(SceneFactory& factory): factory_(factory) {
}

/// @brief シーンを変更します
/// @param name シーン名
void SceneManager::ChangeScene(const std::string& name) {
	if (std::shared_ptr<BaseScene> newScene = factory_.CreateScene(name)) {
		if (currentScene_) {
			currentScene_->Shutdown();
		}
		currentScene_ = newScene;
		currentScene_->Init();
	}
}

/// @brief シーンを更新します
/// @param deltaTime 前フレームからの経過時間（秒）
void SceneManager::Update(const float deltaTime) const {
	if (currentScene_) {
		currentScene_->Update(deltaTime);
	}
}

/// @brief シーンをレンダリングします
void SceneManager::Render() const {
	if (currentScene_) {
		currentScene_->Render();
	}
}

/// @brief 現在のシーンを取得します
std::shared_ptr<BaseScene> SceneManager::GetCurrentScene() const {
	return currentScene_;
}
