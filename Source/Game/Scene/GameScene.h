#pragma once
#include <memory>

#include <Components/PlayerMovement.h>
#include <Components/MeshRenderer/StaticMeshRenderer.h>

#include <Entity/Base/Entity.h>

#include <Object3D/Object3D.h>
#include <Object3D/Object3DCommon.h>

#include <Particle/ParticleObject.h>

#include <Physics/Physics.h>

#include <Renderer/Renderer.h>

#include <Scene/Base/BaseScene.h>

#include <Sprite/Sprite.h>
#include <Sprite/SpriteCommon.h>

#include "Components/ColliderComponent/BoxColliderComponent.h"
#include "Components/ColliderComponent/MeshColliderComponent.h"
#include "CubeMap/CubeMap.h"

#include "Particle/ParticleEmitter.h"
#include "Particle/WindEffect.h"

#include "Physics/PhysicsEngine.h"

class EnemyMovement;
class CameraRotator;
class CameraSystem;

class GameScene : public BaseScene {
public:
	~GameScene() override;
	void Init() override;
	void Update(float deltaTime) override;
	void Render() override;
	void Shutdown() override;

private:
	D3D12* renderer_ = nullptr;

	std::unique_ptr<CubeMap> cubeMap_;

	std::unique_ptr<Entity> cameraRoot_;
	CameraRotator*          cameraRotator_ = nullptr;

	std::unique_ptr<Entity>          camera_;
	std::shared_ptr<CameraComponent> cameraComponent_;

	std::unique_ptr<Entity>               entPlayer_;
	std::shared_ptr<PlayerMovement>       playerMovement_;
	std::shared_ptr<BoxColliderComponent> playerCollider_;

	std::unique_ptr<Entity>             entTestMesh_;
	std::shared_ptr<StaticMeshRenderer> smrTestMesh_;
	StaticMesh*                         debugMesh = nullptr;

	std::unique_ptr<Entity> debugTestMeshEntity_;

	std::unique_ptr<PhysicsEngine> physicsEngine_;

	//std::unique_ptr<ParticleEmitter> mParticleEmitter;

	std::unique_ptr<WindEffect> windEffect_;
};
