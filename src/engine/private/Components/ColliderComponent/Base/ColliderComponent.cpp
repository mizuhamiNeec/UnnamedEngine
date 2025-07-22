#include <engine/public/Components/ColliderComponent/Base/ColliderComponent.h>

#include <engine/public/OldConsole/Console.h>
#include <engine/public/Physics/PhysicsEngine.h>

std::vector<HitResult> ColliderComponent::BoxCast(
	const Vec3& start, const Vec3& direction, const float distance,
	const Vec3& halfSize
) const {
	if (mPhysicsEngine) {
		return mPhysicsEngine->BoxCast(start, direction, distance, halfSize);
	}

	Console::Print(
		"PhysicsEngineがnullptrです\n",
		kConTextColorError,
		Channel::Physics
	);
	return {};
}

std::vector<HitResult> ColliderComponent::RayCast(
	const Vec3& start, const Vec3& direction, float distance) const {
	if (mPhysicsEngine) {
		return mPhysicsEngine->RayCast(start, direction, distance);
	}

	Console::Print(
		"PhysicsEngineがnullptrです\n",
		kConTextColorError,
		Channel::Physics
	);
	return {};
}

PhysicsEngine* ColliderComponent::GetPhysicsEngine() const {
	return mPhysicsEngine;
}
