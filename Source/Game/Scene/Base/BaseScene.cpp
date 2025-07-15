#include <Scene/Base/BaseScene.h>

std::vector<Entity*>& BaseScene::GetEntities() {
	return entities_;
}

void BaseScene::AddEntity(Entity* entity) {
	entities_.emplace_back(entity);
}

void BaseScene::RemoveEntity(Entity* entity) {
	entity->SetParent(nullptr);
	entity->RemoveAllComponents();

	for (auto child : entity->GetChildren()) {
		RemoveEntity(child);
	}

	entities_.erase(
		std::ranges::remove(entities_, entity).begin(),
		entities_.end()
	);

	delete entity;
}

Entity* BaseScene::CreateEntity(const std::string& value) {
	auto newEntity = new Entity(value);
	entities_.emplace_back(newEntity);
	return newEntity;
}
