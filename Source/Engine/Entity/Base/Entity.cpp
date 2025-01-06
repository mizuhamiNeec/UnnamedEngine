#include "Entity.h"

#include <ranges>

#include "Debug/Debug.h"
#include "Lib/Console/ConVarManager.h"

Entity::~Entity() {}

void Entity::Update(const float deltaTime) {
	// 必須コンポーネントの更新
	transform_->Update(deltaTime);

	for (const auto& component : components_ | std::views::values) {
		if (component->IsEditorOnly()/* && !bIsInEditor*/) {
			continue; // エディター専用のコンポーネントはゲーム中には更新しない
		}
		component->Update(deltaTime);
	}

	// 子エンティティの更新
	for (const auto& child : children_) {
		child->Update(deltaTime);
	}

#ifdef _DEBUG
	if (ConVarManager::GetConVar("ent_axis")->GetValueAsBool()) {
		Debug::DrawAxis(GetTransform()->GetWorldPos(), GetTransform()->GetWorldRot());
	}
#endif
}

EntityType Entity::GetType() const {
	return entityType_;
}

void Entity::SetType(const EntityType& type) {
	entityType_ = type;
}

TransformComponent* Entity::GetTransform() const {
	return transform_.get();
}

void Entity::SetParent(Entity* newParent) {
	if (parent_ == newParent) {
		return; // 親が変わらない場合は何もしない
	}

	// 現在の親からこのエンティティを削除
	if (parent_) {
		auto it = std::ranges::remove(parent_->children_, this).begin();
		if (it != parent_->children_.end()) {
			parent_->children_.erase(it);
		}
	}

	// 新しい親を設定
	parent_ = newParent;

	// 新しい親の子供リストに追加
	if (parent_) {
		if (std::ranges::find(parent_->children_, this) == parent_->children_.end()) {
			parent_->children_.push_back(this);
		}
	}

	if (transform_) {
		transform_->MarkDirty();
	}
}

Entity* Entity::GetParent() const {
	return parent_;
}

const std::vector<Entity*>& Entity::GetChildren() const {
	return children_;
}

void Entity::AddChild(Entity* child) {
	if (std::ranges::find(children_, child) == children_.end()) {
		children_.push_back(child);
		child->SetParent(this);
	}
}

void Entity::RemoveChild(Entity* child) {
	auto it = std::ranges::remove(children_, child).begin();
	if (it != children_.end()) {
		children_.erase(it);
		child->SetParent(nullptr);
	}
}

std::string Entity::GetName() {
	return name_;
}

void Entity::SetName(const std::string& name) {
	name_ = name;
}
