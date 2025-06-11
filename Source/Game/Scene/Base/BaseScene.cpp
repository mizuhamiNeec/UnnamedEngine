#include <Scene/Base/BaseScene.h>

std::vector<Entity*>& BaseScene::GetEntities() {
	return entities_;
}

void BaseScene::AddEntity(Entity* entity) {
	entities_.emplace_back(entity);
}

void BaseScene::RemoveEntity(Entity* entity) {
	entities_.erase(
		std::ranges::remove(entities_, entity).begin(),
		entities_.end()
	);
}
