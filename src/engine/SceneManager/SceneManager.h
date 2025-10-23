#pragma once

#include <engine/SceneManager/SceneFactory.h>

/// @brief シーンマネージャークラス
class SceneManager {
public:
	explicit SceneManager(SceneFactory& factory);

	void ChangeScene(const std::string& name);

	void Update(const float deltaTime) const;

	void Render() const;

	std::shared_ptr<BaseScene> GetCurrentScene() const;

private:
	SceneFactory&              factory_;
	std::shared_ptr<BaseScene> currentScene_;
};
