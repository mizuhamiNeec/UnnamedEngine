#pragma once
#include <memory>

#include "../../Engine/Entity/Base/Entity.h"
#include "Base/IGameScene.h"

#include "../Object3D/Object3D.h"
#include "../Object3D/Object3DCommon.h"

#include "../Renderer/Renderer.h"

#include "../Line/Line.h"
#include "../Particle/ParticleObject.h"
#include "../Sprite/Sprite.h"
#include "../Sprite/SpriteCommon.h"

class CameraSystem;

class GameScene : IGameScene {
public:
	void Init(D3D12* renderer, Window* window, SpriteCommon* spriteCommon, ParticleManager* particleManager, Object3DCommon* object3DCommon, ModelCommon* modelCommon, SrvManager* srvManager, EngineTimer* engineTimer) override;
	void Update() override;
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

	std::unique_ptr<ParticleObject> particle_;
};
