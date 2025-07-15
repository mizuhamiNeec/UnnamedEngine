#pragma once
#include <SceneManager/SceneFactory.h>

class SceneManager {
public:
	explicit SceneManager(SceneFactory& factory) : factory_(factory) {}

	void ChangeScene(const std::string& name) {
		if (std::shared_ptr<BaseScene> newScene = factory_.CreateScene(name)) {
			if (currentScene_) {
				currentScene_->Shutdown();
			}
			currentScene_ = newScene;
			currentScene_->Init();
		}
	}

	void Update(const float deltaTime) const {
		if (currentScene_) {
			currentScene_->Update(deltaTime);
		}
	}

	void Render() const {
		if (currentScene_) {
			currentScene_->Render();
		}
	}

	std::shared_ptr<BaseScene> GetCurrentScene() const {
		return currentScene_;
	}

private:
	SceneFactory& factory_;
	std::shared_ptr<BaseScene> currentScene_;
};
