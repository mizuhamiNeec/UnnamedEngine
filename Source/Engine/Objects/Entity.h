// #pragma once
//
// #include "../Components/IComponent.h"
// #include "../Lib/Structs/Structs.h"
//
// class Entity {
// public:
// 	template <typename T>
// 	T* GetComponent() {
// 		for (auto& component : components_) {
// 			if (T* result = dynamic_cast<T*>(component.get())) {
// 				return result;
// 			}
// 		}
// 		return nullptr;
// 	}
//
// 	void AddComponent(std::unique_ptr<IComponent> component) {
// 		components_.emplace_back(std::move(component));
// 	}
//
// 	void ImGuiDraw() {
// 		for (auto& component : components_) {
// 			component->ImGuiDraw();
// 		}
// 	}
//
// private:
// 	std::vector<std::unique_ptr<IComponent>> components_;
// };
