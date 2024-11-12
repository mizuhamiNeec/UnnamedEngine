#pragma once
#include <memory>

#include "IGameScene.h"

#include "../Object3D/Object3D.h"
#include "../Object3D/Object3DCommon.h"

#include "../Renderer/Renderer.h"

#include "../Sprite/Sprite.h"
#include "../Sprite/SpriteCommon.h"
#include "../../RailMover.h"

#include "../../Player.h"

#include "../Particle/ParticleObject.h"

class GameScene : IGameScene {
public:
	void Init(
		D3D12* renderer,
		Window* window,
		SpriteCommon* spriteCommon,
		Object3DCommon* object3DCommon,
		ModelCommon* modelCommon,
		ParticleCommon* particleCommon, EngineTimer* engineTimer
	) override;
	void Update(const float& deltaTime) override;
	void Render() override;
	void Shutdown() override;


	void PlaceRailsAlongSpline();
private:
	Window* window_ = nullptr;
	D3D12* renderer_ = nullptr;

	std::unique_ptr<Object3D> object3D_;
	std::unique_ptr<Sprite> sprite_;
	std::unique_ptr<Model> model_;

	std::unique_ptr<ParticleObject> particle_;

	std::unique_ptr<RailMover> railMover_;

	std::unique_ptr<Object3D> sky_;
	std::unique_ptr<Object3D> world_;
	std::vector<std::unique_ptr<Object3D>> poll_;

	std::unique_ptr<Player> player_;

	std::vector<std::unique_ptr<Object3D>> rails_;
};
