#include "RailCamera.h"
#include <algorithm>
#include <cmath>
#include "Source/Engine/Object3D/Object3D.h"

void RailCamera::Initialize(Object3D* object, const std::vector<Vec3>& points, Vec3 position, Vec3 rotation) {
	object_ = object;

	object_->SetPos(position);
	object_->SetRot(rotation);

	points_ = points;
}

void RailCamera::Update([[maybe_unused]] const float& deltaTime) {

}
