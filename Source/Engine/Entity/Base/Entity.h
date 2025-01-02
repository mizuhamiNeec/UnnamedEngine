#pragma once
#include <memory>
#include <typeindex>
#include <type_traits>
#include <unordered_map>

#include "../Components/Base/Component.h"

enum class EntityType {
	RuntimeOnly,
	EditorOnly,
	Shared,
};

class Entity {
public:
	explicit Entity(const EntityType& type = EntityType::RuntimeOnly) : entityType_(type) {}
	~Entity();

	void Update(float deltaTime);

	[[nodiscard]] EntityType GetType() const { return entityType_; }
	void SetType(const EntityType& type) { entityType_ = type; }

	// コンポーネント
	template <typename T, typename... Args>
	T* AddComponent(Args&&... args);
	template<typename T>
	T* GetComponent();

private:
	std::unordered_map<std::type_index, std::unique_ptr<Component>> components_;
	EntityType entityType_;
};

template <typename T, typename ... Args>
T* Entity::AddComponent(Args&&... args) {
	static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
	auto component = std::make_unique<T>(std::forward<Args>(args)...);
	T* rawPtr = component.get();
	components_[typeid(T)] = std::move(component);
	rawPtr->OnAttach(*this);
	return rawPtr;
}

template <typename T>
T* Entity::GetComponent() {
	auto it = components_.find(typeid(T));
	if (it != components_.end()) {
		return static_cast<T*>(it->second.get());
	}
	return nullptr;
}
