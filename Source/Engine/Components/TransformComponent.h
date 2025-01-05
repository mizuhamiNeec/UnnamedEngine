#pragma once
#include "../Lib/Math/Matrix/Mat4.h"
#include "../Lib/Math/Vector/Vec3.h"
#include "Base/Component.h"

class TransformComponent : public Component {
public:
	TransformComponent() : position_(Vec3::zero),
	                       rotation_(Quaternion::identity),
	                       scale_(Vec3::one),
	                       localMat_(Mat4::identity),
	                       isDirty_(true) {
	}

	void Update(float deltaTime) override;

	// ローカル
	[[nodiscard]] const Vec3& GetLocalPos() const;
	[[nodiscard]] const Quaternion& GetLocalRot() const;
	[[nodiscard]] const Vec3& GetLocalScale() const;
	void SetLocalPos(const Vec3& newPosition);
	void SetLocalRot(const Quaternion& newRotation);
	void SetLocalScale(const Vec3& newScale);

	// ワールド
	[[nodiscard]] Vec3 GetWorldPos() const;
	[[nodiscard]] Quaternion GetWorldRot() const;
	[[nodiscard]] Vec3 GetWorldScale() const;
	void SetWorldPos(const Vec3& newPosition);
	void SetWorldRot(const Quaternion& newRotation);
	void SetWorldScale(const Vec3& newScale);

	const Mat4& GetLocalMat() const;
	[[nodiscard]] Mat4 GetWorldMat() const;

	void DrawInspectorImGui() override;

	void MarkDirty() const;
	Entity* GetOwner() const;

private:
	Vec3 position_;
	Quaternion rotation_;
	Vec3 scale_;

	mutable Mat4 localMat_;
	mutable bool isDirty_;

	void RecalculateMat() const;
};
