#pragma once

#include <optional>
#include <string>

#include <engine/SceneManager/SceneFactory.h>

/// @brief シーンマネージャークラス
class SceneManager {
public:
	explicit SceneManager(SceneFactory& factory);

	/// @brief 即時シーン遷移（直接呼ぶ場合はフレーム境界を考慮すること）
	void ChangeScene(const std::string& name);

	/// @brief シーン遷移をリクエストする（遅延実行される）
	void RequestSceneChange(const std::string& name);

	/// @brief ペンディング中のシーン遷移を処理する（Engine::Tick末尾で呼ぶ）
	void ProcessPendingSceneChange();

	/// @brief 現在シーンを終了して破棄する（エンジン終了時向け）
	void ShutdownCurrentScene();

	void Update(float deltaTime) const;

	void Render() const;

	std::shared_ptr<BaseScene> GetCurrentScene() const;

	/// @brief 現在のシーン名を取得する
	[[nodiscard]] const std::string& GetCurrentSceneName() const { return mCurrentSceneName; }

private:
	SceneFactory&              mFactory;
	std::shared_ptr<BaseScene> mCurrentScene;
	std::string                mCurrentSceneName;
	std::optional<std::string> mPendingSceneName;
};
