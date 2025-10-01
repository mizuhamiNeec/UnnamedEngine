#include <engine/Components/ColliderComponent/Base/ColliderComponent.h>

#include <engine/OldConsole/Console.h>

PhysicsEngine* ColliderComponent::GetPhysicsEngine() const {
	return mPhysicsEngine;
}
