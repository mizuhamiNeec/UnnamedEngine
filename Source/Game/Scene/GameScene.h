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
	static void RotateMesh(Quaternion& prev, Quaternion target, float deltaTime, Vec3 velocity);
	void Update(float deltaTime) override;
	void Render() override;
	void Shutdown() override;

private:
	static bool CheckSphereCollision(const ColliderComponent& collider1, const ColliderComponent& collider2);

	void CheckSwordCollision(Entity* player, Entity* enemy);

	struct Number {
		std::unique_ptr<Sprite> sprite;
		Vec2 pos = Vec2::zero;
		Vec2 uvSize = { 0.1f, 1.0f };
		Vec2 uvPos = { 0.0f,0.0f };
	};
	void NumbersUpdate(int number, std::vector<Number>& digits, Vec3 offset);
	std::vector<Number> digits_;

	Window* window_ = nullptr;
	D3D12* renderer_ = nullptr;

	std::unique_ptr<Sprite> hudSprite_;
	std::unique_ptr<Sprite> comboSprite_;

	int combo_ = 0;

	std::unordered_map<Entity*, float> lastSwordHitTime_;
	float kSwordHitInterval = 0.5f;

	// Env
	std::unique_ptr<Object3D> ground_;

	// Shadow
	std::unique_ptr<Object3D> shadow_;

	// Player
	std::unique_ptr<Entity> player_;
	PlayerMovement* playerMovementComponent_ = nullptr;
	ColliderComponent* playerColliderComponent_ = nullptr;
	std::unique_ptr<Object3D> playerObject_;
	Quaternion previousRotation_ = Quaternion::identity;
	Quaternion targetRotation_ = Quaternion::identity;

	// 複数の敵
	std::vector<std::unique_ptr<Entity>> enemies_;
	std::vector<EnemyMovement*> enemyMovementComponents_;
	std::vector<ColliderComponent*> enemyColliderComponents_;
	std::vector<std::unique_ptr<Object3D>> enemyObjects_;
	std::vector<Quaternion> enemyPreviousRotations_;
	std::vector<Quaternion> enemyTargetRotations_;

	// 剣
	std::unique_ptr<Entity> swordEntity_;
	std::unique_ptr<Object3D> sword_;
	TransformComponent* swordTransform_ = nullptr;

	std::unique_ptr<Entity> cameraRoot_;
	CameraRotator* cameraRotator_ = nullptr;

	std::unique_ptr<Entity> camera_;
	std::shared_ptr<CameraComponent> cameraComponent_;

	std::unique_ptr<ParticleObject> particle_;
};
