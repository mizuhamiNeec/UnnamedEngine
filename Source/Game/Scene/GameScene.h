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

	Window* window_ = nullptr;
	D3D12* renderer_ = nullptr;

	std::unique_ptr<Sprite> sprite_;

	// Env
	std::unique_ptr<Object3D> ground_;

	// Shadow
	std::unique_ptr<Object3D> shadow_;

	std::unique_ptr<Entity> testEnt_;
	std::unique_ptr<Entity> testEnt2_;
	std::unique_ptr<Entity> testEnt3_;

	ColliderComponent* testEntColliderComponent_ = nullptr;

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

	std::unique_ptr<Entity> cameraRoot_;
	CameraRotator* cameraRotator_ = nullptr;

	std::unique_ptr<Entity> camera_;
	std::shared_ptr<CameraComponent> cameraComponent_;

	std::unique_ptr<ParticleObject> particle_;

	// 剣のビヘイビア
	enum class SwordBehavior {
		Idle,
		Attack1,
		Attack2,
		Attack3,
	};

	void RequestBehavior(SwordBehavior request);

	void IdleInit();
	void IdleUpdate(float deltaTime);

	void Attack1Init();
	void Attack1Update(float deltaTime);

	void Attack2Init();
	void Attack2Update(float deltaTime);

	void Attack3Init();
	void Attack3Update(float deltaTime);

	void BehaviorInit();
	void BehaviorUpdate(float deltaTime);

	SwordBehavior behavior_ = SwordBehavior::Idle;
	std::optional<SwordBehavior> behaviorRequest_ = std::nullopt;
};
