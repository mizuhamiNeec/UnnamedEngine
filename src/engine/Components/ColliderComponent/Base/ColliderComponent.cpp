#include <engine/Components/ColliderComponent/Base/ColliderComponent.h>

#include <engine/OldConsole/Console.h>

/// @brief 物理エンジンを取得します
/// @return 物理エンジンのポインタ
PhysicsEngine* ColliderComponent::GetPhysicsEngine() const {
	return mPhysicsEngine;
}
