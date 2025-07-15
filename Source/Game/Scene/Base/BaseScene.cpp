#include <Scene/Base/BaseScene.h>

std::vector<Entity*>& BaseScene::GetEntities() {
	return mEntities;
}

void BaseScene::AddEntity(Entity* entity) {
	mEntities.emplace_back(entity);
}

void BaseScene::RemoveEntity(Entity* entity) {
	entity->SetParent(nullptr);
	entity->RemoveAllComponents();

	for (auto child : entity->GetChildren()) {
		RemoveEntity(child);
	}

	mEntities.erase(
		std::ranges::remove(mEntities, entity).begin(),
		mEntities.end()
	);

	delete entity;
}

Entity* BaseScene::CreateEntity(const std::string& value) {
	auto newEntity = new Entity(value);
	mEntities.emplace_back(newEntity);
	return newEntity;
}
