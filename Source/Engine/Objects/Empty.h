#pragma once

#include "../Lib/Structs/Structs.h"

class Empty {
public:
	void Update();

	void AddChild(Empty* child);

	// Setter
	void SetTransform(const Transform& newTransform);
	void SetPos(const Vec3& newPos);
	void SetRot(const Vec3& newRot);
	void SetScale(const Vec3& newScale);
	void SetParent(Empty* parent);

	// Getter
	Transform& GetTransform();
	Vec3& GetPos();
	Vec3& GetRot();
	Vec3& GetScale();
	[[nodiscard]] Empty* GetParent() const;

private:
	Transform transform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	Empty* parent_ = nullptr;
	std::vector<Empty*> children_;
};
