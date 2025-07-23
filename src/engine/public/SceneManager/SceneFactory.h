#pragma once
#include <memory>
#include <string>
#include <functional>

#include <game/public/scene/base/BaseScene.h>

class SceneFactory {
public:
	template <typename T>
	void RegisterScene(const std::string& name) {
		sceneCreators_[name] = []() -> std::shared_ptr<BaseScene> {
			return std::make_shared<T>();
		};
	}

	std::shared_ptr<BaseScene> CreateScene(const std::string& name) {
		auto it = sceneCreators_.find(name);
		if (it != sceneCreators_.end()) {
			return it->second();
		}
		return nullptr;
	}

private:
	std::unordered_map<std::string, std::function<std::shared_ptr<BaseScene>()>>
	sceneCreators_;
};
