#pragma once
#include <memory>

#include "IGameScene.h"
#include "../Engine/Line/LineCommon.h"

#include "../Object3D/Object3D.h"
#include "../Object3D/Object3DCommon.h"

#include "../Renderer/Renderer.h"

#include "../Line/Line.h"
#include "../Particle/ParticleObject.h"
#include "../Sprite/Sprite.h"
#include "../Sprite/SpriteCommon.h"

class GameScene : IGameScene {
public:
	void Init(
		D3D12* renderer,
		Window* window,
		SpriteCommon* spriteCommon,
		Object3DCommon* object3DCommon,
		ModelCommon* modelCommon,
		ParticleCommon* particleCommon,
		EngineTimer* engineTimer
	) override;
	void Update() override;
	void Render() override;
	void Shutdown() override;

private:
	Window* window_ = nullptr;
	D3D12* renderer_ = nullptr;

	std::unique_ptr<Object3D> object3D_;
	std::unique_ptr<Sprite> sprite_;
	std::unique_ptr<Model> model_;

	std::unique_ptr<Line> line_;

	std::unique_ptr<ParticleObject> particle_;

	Transform transform_;
	Transform cameraTransform_;
};
