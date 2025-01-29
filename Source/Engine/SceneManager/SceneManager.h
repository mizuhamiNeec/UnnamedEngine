#pragma once
#include <SceneManager/SceneFactory.h>

class SceneManager {
public:
	explicit SceneManager(SceneFactory& factory) : factory_(factory) {}

	void ChangeScene(const std::string& name) {
		std::shared_ptr<Scene> newScene = factory_.CreateScene(name);

		if (newScene) {
			if (currentScene_) {
				currentScene_->Shutdown();
			}
			currentScene_ = newScene;
			currentScene_->Init();
		}
	}

	void Update(float deltaTime) {
		if (currentScene_) {
			currentScene_->Update(deltaTime);
		}
	}

	void Render() {
		if (currentScene_) {
			currentScene_->Render();
		}
	}

	std::shared_ptr<Scene> GetCurrentScene() const {
		return currentScene_;
	}

private:
	SceneFactory& factory_;
	std::shared_ptr<Scene> currentScene_;
};
