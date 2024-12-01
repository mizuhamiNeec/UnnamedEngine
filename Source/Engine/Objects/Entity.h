#pragma once

#include "../Lib/Structs/Structs.h"

class Entity {
public:
	void Update();

	void AddChild(Entity* child);

	// Setter
	void SetTransform(const Transform& newTransform);
	void SetPosition(const Vec3& newPos);
	void SetRotation(const Vec3& newRot);
	void SetScale(const Vec3& newScale);
	void SetParent(Entity* parent);

	// Getter
	Transform& GetTransform();
	Vec3& Position();
	Vec3& Rotation();
	Vec3& Scale();
	[[nodiscard]] Entity* GetParent() const;

private:
	Transform transform_{
		.scale = Vec3::one,
		.rotate = Vec3::zero,
		.translate = Vec3::zero
	};
	Entity* parent_ = nullptr;
	std::vector<Entity*> children_;
};
