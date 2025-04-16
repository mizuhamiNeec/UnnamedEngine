#include <Scene/Base/BaseScene.h>

std::vector<Entity*>& BaseScene::GetEntities() {
	return entities_;
}

void BaseScene::AddEntity(Entity* entity) {
	entities_.emplace_back(entity);
}
