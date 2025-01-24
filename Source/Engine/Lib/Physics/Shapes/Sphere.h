#pragma once
#include <Lib/Physics/Shapes/Base/Shape.h>

//-----------------------------------------------------------------------------
// 衝突形状: 球
//-----------------------------------------------------------------------------
class Sphere : public Shape {
public:
	[[nodiscard]] Physics::AABB GetBoundingBox() const override;
	[[nodiscard]] bool IsColliding(const Shape& other) const override;
private:
	Vec3 center_;
	float radius_;
};
