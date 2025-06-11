#pragma once
#include <algorithm>
#include <memory>
#include <ranges>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>

#include <Components/Base/Component.h>
#include <Components/Transform/TransformComponent.h>

enum class EntityType {
	RuntimeOnly, // ゲーム中のみ
	EditorOnly,	 // エディターのみ
	Shared,		 // ゲーム中とエディターの両方
};

class Entity {
public:
	explicit Entity(const std::string& name, const EntityType& type = EntityType::RuntimeOnly) :
		transform_(std::make_unique<TransformComponent>()),
		entityType_(type),
		name_(name) {
		transform_->OnAttach(*this);
	}

	~Entity();

	void Update(float deltaTime);
	void Render(ID3D12GraphicsCommandList* commandList) const;

	[[nodiscard]] EntityType GetType() const;
	void SetType(const EntityType& type);

	[[nodiscard]] TransformComponent* GetTransform() const;

	[[nodiscard]] bool IsActive() const;
	void SetActive(bool active);
	[[nodiscard]] bool IsVisible() const;
	void SetVisible(bool visible);

	// コンポーネント
	template <typename T, typename... Args>
	T* AddComponent(Args &&...args);
	template <typename T>
	T* GetComponent();

	// すべてのコンポーネントを取得
	template <typename T>
	std::vector<T*> GetComponents();

	// 親子関係
	void SetParent(Entity* newParent);
	[[nodiscard]] Entity* GetParent() const;
	[[nodiscard]] const std::vector<Entity*>& GetChildren() const;
	void AddChild(Entity* child);
	void RemoveChild(Entity* child);

	std::string& GetName();
	void        SetName(const std::string& name);

	void RemoveAllComponents();

private:
	Entity* parent_ = nullptr;
	std::vector<Entity*> children_;

	std::unique_ptr<TransformComponent> transform_;
	std::vector<std::unique_ptr<Component>> components_;
	EntityType entityType_; // エンティティの種類
	std::string name_; // エンティティの名前

	bool bIsActive_ = true; // Updateを呼ぶかどうか
	bool bIsVisible_ = true; // 描画を行うかどうか
};

template <typename T, typename... Args>
T* Entity::AddComponent(Args &&...args) {
	static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
	auto component = std::make_unique<T>(std::forward<Args>(args)...);
	T* rawPtr = component.get();
	components_.emplace_back(std::move(component));
	rawPtr->OnAttach(*this);
	return rawPtr;
}

template <typename T>
T* Entity::GetComponent() {
	for (const auto& component : components_) {
		if (auto* castedComponent = dynamic_cast<T*>(component.get())) {
			return castedComponent;
		}
	}
	return nullptr;
}

template <typename T>
std::vector<T*> Entity::GetComponents() {
	static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
	std::vector<T*> result;
	for (const auto& component : components_) {
		if (auto castedComponent = dynamic_cast<T*>(component.get())) {
			result.emplace_back(castedComponent);
		}
	}
	return result;
}