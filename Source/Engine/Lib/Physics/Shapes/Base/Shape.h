#pragma once

#include <Lib/Physics/Physics.h>

//-----------------------------------------------------------------------------
// 衝突形状の基底クラス
//-----------------------------------------------------------------------------
class Shape {
public:
	[[nodiscard]] virtual Physics::AABB GetBoundingBox() const = 0;
	[[nodiscard]] virtual bool IsColliding(const Shape& other) const = 0;
};
