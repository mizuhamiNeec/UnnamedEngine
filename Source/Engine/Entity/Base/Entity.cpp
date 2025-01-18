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

	if (ConVarManager::GetConVar("ent_axis")->GetValueAsBool()) {
		Debug::DrawAxis(GetTransform()->GetWorldPos(), GetTransform()->GetWorldRot());
	}
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
	// 循環参照チェック
	Entity* check = newParent;
	while (check) {
		if (check == this) {
			Console::Print(std::format("Entity '{}': Circular parenting detected!", name_),
				Vec4(1, 0, 0, 1), Channel::General);
			return;
		}
		check = check->parent_;
	}

	if (parent_ == newParent) {
		return; // 変更なし
	}

	// 現在のワールド変換を保存
	Vec3 currentWorldPos;
	Quaternion currentWorldRot;
	Vec3 currentWorldScale;

	if (transform_) {
		currentWorldPos = transform_->GetWorldPos();
		currentWorldRot = transform_->GetWorldRot();
		currentWorldScale = transform_->GetWorldScale();
	}

	// 既存の親から削除
	if (parent_) {
		auto it = std::find(parent_->children_.begin(), parent_->children_.end(), this);
		if (it != parent_->children_.end()) {
			parent_->children_.erase(it);
		}
	}

	// 新しい親を設定
	parent_ = newParent;

	// トランスフォームの更新
	if (transform_) {
		if (parent_ && parent_->GetTransform()) {
			// 親のワールド変換を取得
			Vec3 parentWorldPos = parent_->GetTransform()->GetWorldPos();
			Quaternion parentWorldRot = parent_->GetTransform()->GetWorldRot();
			Vec3 parentWorldScale = parent_->GetTransform()->GetWorldScale();

			// 位置の計算
			Vec3 localPos = currentWorldPos - parentWorldPos;
			// 親の回転の逆回転を適用
			localPos = parentWorldRot.Inverse().RotateVector(localPos);
			// スケールの補正
			localPos = localPos / parentWorldScale;

			// 回転の計算
			Quaternion localRot = parentWorldRot.Inverse() * currentWorldRot;

			// スケールの計算
			Vec3 localScale = currentWorldScale / parentWorldScale;

			transform_->SetLocalPos(localPos);
			transform_->SetLocalRot(localRot);
			transform_->SetLocalScale(localScale);
		} else {
			transform_->SetWorldPos(currentWorldPos);
			transform_->SetWorldRot(currentWorldRot);
			transform_->SetWorldScale(currentWorldScale);
		}

		transform_->MarkDirty();

		// 子のトランスフォーム更新
		for (auto* child : children_) {
			if (child->transform_) {
				child->transform_->MarkDirty();
			}
		}
	}

	// 新しい親の子リストに追加
	if (parent_) {
		parent_->children_.push_back(this);
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
