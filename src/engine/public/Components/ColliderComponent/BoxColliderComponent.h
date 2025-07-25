#pragma once

#include <engine/public/Components/ColliderComponent/Base/ColliderComponent.h>

#include <engine/public/Physics/Physics.h>

class SceneComponent;

class BoxColliderComponent : public ColliderComponent {
public:
	BoxColliderComponent();
	~BoxColliderComponent() override = default;

	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

	bool CheckCollision(const ColliderComponent* other) const override;
	bool IsDynamic() override;

	// コライダーのサイズを設定
	[[nodiscard]] const Vec3& GetSize() const;
	void SetSize(const Vec3& size);

	[[nodiscard]] const Vec3& GetOffset() const { return offset_; }
	void SetOffset(const Vec3& offset) { offset_ = offset; }

	[[nodiscard]] AABB GetBoundingBox() const override;
	// bool CheckCollision(const ColliderComponent& other) const override;

private:
	SceneComponent* transform_ = nullptr;

	Vec3 size_ = Vec3::one;
	Vec3 offset_ = Vec3::zero;
};
