#pragma once
#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>

#include <Components/Base/Component.h>
#include <Components/Transform/TransformComponent.h>

enum class EntityType {
	RuntimeOnly,
	EditorOnly,
	Shared,
};

class Entity {
public:
	explicit Entity(std::string name, const EntityType& type = EntityType::RuntimeOnly) : transform_(
		std::make_unique<TransformComponent>()
	),
		entityType_(type),
		name_(std::move(name)) {
		transform_->OnAttach(*this);
	}

	~Entity();

	void Update(float deltaTime);
	void Render(ID3D12GraphicsCommandList* commandList);

	[[nodiscard]] EntityType GetType() const;
	void SetType(const EntityType& type);

	[[nodiscard]] TransformComponent* GetTransform() const;

	// コンポーネント
	template <typename T, typename... Args>
	T* AddComponent(Args&&... args);
	template <typename T>
	T* GetComponent();

	// 親子関係
	void SetParent(Entity* newParent);
	[[nodiscard]] Entity* GetParent() const;
	[[nodiscard]] const std::vector<Entity*>& GetChildren() const;
	void AddChild(Entity* child);
	void RemoveChild(Entity* child);

	std::string GetName();
	void SetName(const std::string& name);

private:
	Entity* parent_ = nullptr;
	std::vector<Entity*> children_;

	std::unique_ptr<TransformComponent> transform_;
	std::unordered_map<std::type_index, std::unique_ptr<Component>> components_;
	EntityType entityType_;
	std::string name_;
};

template <typename T, typename... Args>
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
