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
	bool IsLineSphereColliding(const Vec3& lineStart, const Vec3& lineDir, const Vec3& sphereCenter,
		float sphereRadius);

private:
	Window* window_ = nullptr;
	D3D12* renderer_ = nullptr;

	float kMaxHeat = 100.0f;
	float kMinHeat = 0.0f;
	float kOverheatPenaltyTime = 3.0f;
	float heat = kMaxHeat;
	bool isShooting = false;
	bool isOverheated = false;
	float lastShootTime = 0.0f;
	float overheatStartTime = 0.0f;

	float currentTime_ = 0.0f;

	Vec3 reticlePos_ = { kClientWidth * 0.5f,kClientHeight * 0.5f ,0.0f };
	Vec3 reticleMoveInput_ = Vec3::zero;
	Vec3 reticleMoveVel_ = Vec3::zero;
	float reticleMoveSpd_ = 640.0f;

	std::unique_ptr<Object3D> object3D_;
	std::unique_ptr<Sprite> reticleSprite_;
	std::unique_ptr<Sprite> gaugeFrame_;
	std::unique_ptr<Sprite> gaugeBar_;
	std::unique_ptr<Model> model_;

	std::unique_ptr<ParticleObject> particle_;

	std::unique_ptr<RailMover> railMover_;

	std::unique_ptr<Object3D> sky_;
	std::unique_ptr<Object3D> world_;
	std::unique_ptr<Object3D> axis_;
	std::unique_ptr<Object3D> line_;
	std::vector<std::unique_ptr<Object3D>> poll_;

	std::unique_ptr<Player> player_;

	std::vector<std::unique_ptr<Object3D>> rails_;
	std::vector<std::unique_ptr<Object3D>> mato_;
};
