#pragma once
#include <memory>

#include <Entity/Base/Entity.h>

#include <Object3D/Object3D.h>
#include <Object3D/Object3DCommon.h>

#include <Particle/ParticleObject.h>

#include <Renderer/Renderer.h>

#include <Scene/Base/Scene.h>

#include <Components/MeshRenderer/StaticMeshRenderer.h>
#include <Sprite/Sprite.h>
#include <Sprite/SpriteCommon.h>
#include <Components/PlayerMovement.h>

class EnemyMovement;
class CameraRotator;
class CameraSystem;

class GameScene : public Scene {
public:
	~GameScene() override = default;
	void Init(Engine* engine) override;
	void Update(float deltaTime) override;
	void Render() override;
	void Shutdown() override;

private:
	D3D12* renderer_ = nullptr;

	std::unique_ptr<Sprite> sprite_;

	std::unique_ptr<Entity> cameraRoot_;
	CameraRotator* cameraRotator_ = nullptr;

	std::unique_ptr<Entity> camera_;
	std::shared_ptr<CameraComponent> cameraComponent_;

	std::unique_ptr<Entity> entPlayer_;
	std::shared_ptr<PlayerMovement> playerMovement_;

	std::unique_ptr<Entity> testMeshEntity_;
	std::shared_ptr<StaticMeshRenderer> floatTestMR_;
	std::unique_ptr<Entity> debugTestMeshEntity_;
	std::shared_ptr<StaticMeshRenderer> debugTestMR_;
};
