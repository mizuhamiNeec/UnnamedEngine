#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>

#include <engine/SceneManager/SceneFactory.h>
#include <engine/Sprite/Sprite.h>

/// @brief シーンマネージャークラス
class SceneManager {
public:
	explicit SceneManager(SceneFactory& factory);

	/// @brief 即時シーン遷移（直接呼ぶ場合はフレーム境界を考慮すること）
	void ChangeScene(const std::string& name);

	/// @brief シーン遷移をリクエストする（遅延実行される）
	void RequestSceneChange(const std::string& name);

	/// @brief 現在シーンの再読込をリクエストする（遅延実行される）
	void RequestCurrentSceneReload();

	/// @brief ペンディング中のシーン遷移を処理する（Engine::Tick末尾で呼ぶ）
	void ProcessPendingSceneChange();

	/// @brief 現在シーンを終了して破棄する（エンジン終了時向け）
	void ShutdownCurrentScene();

	void Update(float deltaTime);

	void Render() const;

	std::shared_ptr<BaseScene> GetCurrentScene() const;

	/// @brief 現在のシーン名を取得する
	[[nodiscard]] const std::string& GetCurrentSceneName() const { return mCurrentSceneName; }

private:
	enum class TransitionPhase {
		None,
		Exit,
		Enter
	};

	void EnsureTransitionSprites();
	void UpdateTransitionOverlay();
	void DrawTransitionOverlay() const;
	void BeginSceneTransition(const std::string& name);
	bool IsTransitionActive() const;

	SceneFactory&              mFactory;
	std::shared_ptr<BaseScene> mCurrentScene;
	std::string                mCurrentSceneName;
	std::optional<std::string> mPendingSceneName;
	std::optional<std::string> mTransitionTargetSceneName;
	bool                       mPendingCurrentSceneReload = false;

	TransitionPhase mTransitionPhase         = TransitionPhase::None;
	float           mTransitionElapsedSec    = 0.0f;
	float           mTransitionCoverProgress = 0.0f;
	bool            mPendingTransitionSwap   = false;

	std::unique_ptr<Sprite>            mTransitionBackdropSprite;
	std::array<std::unique_ptr<Sprite>, 4> mTransitionPanelSprites;
};
