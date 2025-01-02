#pragma once

#include "../../Engine/Renderer/D3D12.h"
#include "../Model/ModelCommon.h"
#include "../Object3D/Object3DCommon.h"
#include "../Particle/ParticleManager.h"
#include "../Renderer/Renderer.h"
#include "../Sprite/SpriteCommon.h"

class EntityManager;
class EngineTimer;

class IGameScene {
public:
	virtual ~IGameScene() = default;
	virtual void Init(
		D3D12* renderer,
		Window* window,
		SpriteCommon* spriteCommon,
		ParticleManager* particleManager,
		Object3DCommon* object3DCommon,
		ModelCommon* modelCommon,
		SrvManager* srvManager,
		EngineTimer* engineTimer) = 0;
	virtual void Update() = 0;
	virtual void Render() = 0;
	virtual void Shutdown() = 0;

protected:
	SpriteCommon* spriteCommon_ = nullptr;
	ParticleManager* particleManager_ = nullptr;
	Object3DCommon* object3DCommon_ = nullptr;
	ModelCommon* modelCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	EngineTimer* timer_ = nullptr;
};
