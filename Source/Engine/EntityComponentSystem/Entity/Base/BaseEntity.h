#pragma once

#include <memory>

#include "../../Components/Base/BaseComponent.h"

#include "../Lib/Structs/Structs.h"

class BaseEntity {
public:
	template <typename T>
	T* GetComponent() {
		for (auto& component : components_) {
			if (T* result = dynamic_cast<T*>(component.get())) {
				return result;
			}
		}
		return nullptr;
	}

	template <typename T, typename... Args>
	T* AddComponent(Args&&... args) {
		static_assert(std::is_base_of_v<BaseComponent, T>, "T must derive from BaseComponent");
		auto component = std::make_unique<T>(this, std::forward<Args>(args)...);
		T* ptr = component.get();
		components_.push_back(std::move(component));
		return ptr;
	}

	void ImGuiDraw() const {
		for (auto& component : components_) {
			component->ImGuiDraw();
		}
	}

private:
	std::vector<std::unique_ptr<BaseComponent>> components_;
};
