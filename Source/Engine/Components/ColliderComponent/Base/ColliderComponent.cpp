#include <Components/ColliderComponent/Base/ColliderComponent.h>

#include <Physics/PhysicsEngine.h>

std::vector<HitResult> ColliderComponent::BoxCast(
	const Vec3& start, const Vec3& direction, float distance, const Vec3& halfSize
) const {
	if (physicsEngine_) {
		return physicsEngine_->BoxCast(start, direction, distance, halfSize);
	}

	Console::Print(
		"PhysicsEngineがnullptrです\n",
		kConTextColorError,
		Channel::Physics
	);
	return {};
}
