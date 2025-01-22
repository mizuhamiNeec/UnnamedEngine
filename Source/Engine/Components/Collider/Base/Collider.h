#pragma once
#include "Components/Base/Component.h"

enum class ColliderType {
	AABB,
	OBB,
	Sphere,
	Capsule,
	Mesh
};

class Collider : public Component {
public:
	void Update(float deltaTime) override = 0;
	void DrawInspectorImGui() override = 0;

	// コライダー形状の取得
	virtual ColliderType GetColliderType() = 0;
protected:
	ColliderType colliderType_ = ColliderType::Sphere;
};

