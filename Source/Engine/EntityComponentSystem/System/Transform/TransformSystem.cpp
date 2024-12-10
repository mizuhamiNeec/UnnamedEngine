#include "TransformSystem.h"

#include "../../../Debug/Debug.h"

void TransformSystem::RegisterComponent(TransformComponent* component) {
	if (std::ranges::find(components_, component) == components_.end()) {
		components_.push_back(component);
	}
}

void TransformSystem::UnregisterComponent(TransformComponent* component) {
	auto it = std::ranges::find(components_, component);
	if (it != components_.end()) {
		components_.erase(it);
	}
}

void TransformSystem::Initialize() {
}

void TransformSystem::Update([[maybe_unused]] float deltaTime) {// 登録されたコンポーネントを更新
	for (TransformComponent* component : components_) {
		if (!component->GetParent()) {
			component->UpdateMatrix();
		}
		Debug::DrawAxis(component->GetWorldPosition(), component->GetWorldRotation());
	}
}

void TransformSystem::Terminate() {
	components_.clear();
}
