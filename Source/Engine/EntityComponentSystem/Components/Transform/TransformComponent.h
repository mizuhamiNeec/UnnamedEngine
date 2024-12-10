#pragma once
#include <iosfwd>
#include <vector>

#include "../Lib/Math/Matrix/Mat4.h"
#include "../Lib/Math/Quaternion/Quaternion.h"
#include "../Lib/Math/Vector/Vec3.h"

#include "../Base/BaseComponent.h"

class BaseEntity;

class TransformComponent : public BaseComponent {
public:
	//-------------------------------------------------------------------------
	// Func
	//-------------------------------------------------------------------------

	// コンストラクタ
	TransformComponent(BaseEntity* owner);

	// BaseComponent
	void Initialize() override;
	void Update(float deltaTime) override;
	void Terminate() override;
	void Serialize(std::ostream& out) const override;
	void Deserialize(std::istream& in) override;
	void ImGuiDraw() override;

	void Translate(const Vec3& translation);
	void Rotate(const Quaternion& rotation);

	// 親子関係
	void SetParent(TransformComponent* newParent);
	void AddChild(TransformComponent* child);
	void RemoveChild(TransformComponent* child);
	void DetachFromParent();

	// ローカル/ワールド変換
	Vec3 TransformPoint(const Vec3& point) const;
	Vec3 TransformDirection(const Vec3& direction) const;
	Vec3 InverseTransformPoint(const Vec3& point) const;
	Vec3 InverseTransformDirection(const Vec3& direction) const;

	// 行列更新
	void UpdateMatrix();

	// 更新を行わせる
	void MarkDirty();

	//-------------------------------------------------------------------------
	// Getters / Setters
	//-------------------------------------------------------------------------

	// 親子関係
	TransformComponent* GetParent() const;
	const std::vector<TransformComponent*>& GetChildren() const;

	// 行列のゲッター
	const Mat4& GetLocalToWorldMatrix() const;
	const Mat4& GetWorldToLocalMatrix() const;

	// トランスフォーム
	const Vec3& GetLocalPosition() const;
	const Quaternion& GetLocalRotation() const;
	const Vec3& GetLocalScale() const;
	const Vec3& GetWorldPosition() const;
	const Quaternion& GetWorldRotation() const;
	const Vec3& GetWorldScale() const;
	void SetWorldPos(const Vec3& newWorldPos);
	void SetWorldRot(const Quaternion& newWorldRot);
	void SetWorldScale(const Vec3& newWorldScale);
	void SetLocalPos(const Vec3& newLocalPos);
	void SetLocalRot(const Quaternion& newLocalRot);
	void SetLocalScale(const Vec3& newLocalScale);

private:
	// 変換情報
	Vec3 localPos_;
	Quaternion localRot_;
	Vec3 localScale_;
	Vec3 pos_;
	Quaternion rot_;
	Vec3 scale_;

	// 階層構造
	TransformComponent* parent_;
	std::vector<TransformComponent*> children_;

	// 行列
	Mat4 localToWorldMat_;
	Mat4 worldToLocalMat_;
	bool isDirty_;
};
