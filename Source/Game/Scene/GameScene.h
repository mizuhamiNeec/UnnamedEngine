#pragma once
#include <memory>

#include "Entity/Base/Entity.h"
#include "Base/Scene.h"
#include "Components/ColliderComponent.h"

#include "Object3D/Object3D.h"
#include "Object3D/Object3DCommon.h"

#include "Renderer/Renderer.h"

#include "Particle/ParticleObject.h"
#include "Sprite/Sprite.h"
#include "Sprite/SpriteCommon.h"

class EnemyMovement;
class CameraRotator;
class PlayerMovement;
class CameraSystem;

class GameScene : public Scene {
public:
	~GameScene() override = default;
	void Init(Engine* engine) override;
	void Update(float deltaTime) override;
	void Render() override;
	void Shutdown() override;

private:
	Window* window_ = nullptr;
	D3D12* renderer_ = nullptr;

	std::unique_ptr<Entity> cameraRoot_;
	CameraRotator* cameraRotator_ = nullptr;

	std::unique_ptr<Entity> camera_;
	std::shared_ptr<CameraComponent> cameraComponent_;

	std::unique_ptr<ParticleObject> particle_;
};
