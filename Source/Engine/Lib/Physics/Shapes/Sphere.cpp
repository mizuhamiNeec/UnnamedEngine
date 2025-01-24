#include "Sphere.h"

Physics::AABB Sphere::GetBoundingBox() const {
	return {
		center_ - Vec3(radius_),
		center_ + Vec3(radius_)
	};
}

bool Sphere::IsColliding(const Shape& other) const {
	other;
	return false;
}
