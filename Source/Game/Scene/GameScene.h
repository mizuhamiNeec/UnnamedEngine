#pragma once
#include <memory>

#include "Entity/Base/Entity.h"
#include "Base/Scene.h"

#include "Object3D/Object3D.h"
#include "Object3D/Object3DCommon.h"

#include "Renderer/Renderer.h"

#include "Particle/ParticleObject.h"
#include "Sprite/Sprite.h"
#include "Sprite/SpriteCommon.h"

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

	std::unique_ptr<Camera> mainCamera_;

	std::unique_ptr<Object3D> object3D_;
	std::unique_ptr<Sprite> sprite_;
	std::unique_ptr<Model> model_;

	std::unique_ptr<Entity> testEnt_;
	std::unique_ptr<Entity> testEnt2_;
	std::unique_ptr<Entity> testEnt3_;

	std::unique_ptr<Entity> player_;
	PlayerMovement* playerMovementComponent_ = nullptr;

	std::unique_ptr<Entity> cameraBase_;
	CameraRotator* cameraRotator_ = nullptr;

	std::unique_ptr<Entity> camera_;
	std::shared_ptr<CameraComponent> cameraComponent_;

	std::unique_ptr<ParticleObject> particle_;
};
