#include <Scene/Base/Scene.h>

std::vector<Entity*>& Scene::GetEntities() {
	return entities_;
}

void Scene::AddEntity(Entity* entity) {
	entities_.push_back(entity);
}
