#include "Entity.h"

#include <ranges>

Entity::~Entity() {
}

void Entity::Update(const float deltaTime) {
	for (const auto& component : components_ | std::views::values) {
		if (component->IsEditorOnly()/* && !bIsInEditor*/) {
			continue; // エディター専用のコンポーネントはゲーム中には更新しない
		}
		component->Update(deltaTime);
	}
}
