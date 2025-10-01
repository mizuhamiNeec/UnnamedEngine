#include <engine/public/Components/ColliderComponent/Base/ColliderComponent.h>

#include <engine/public/OldConsole/Console.h>

PhysicsEngine* ColliderComponent::GetPhysicsEngine() const {
	return mPhysicsEngine;
}
