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

	std::unique_ptr<Sprite> titleSprite_;
	std::unique_ptr<Sprite> result_;

	float timer = 10.0f;
	float time = 0.0f;
	bool isPlaying = false;

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

	// 剣のビヘイビア
	enum class SwordBehavior {
		Idle,
		Attack1,
		Attack2,
		Attack3Swing,
		Attack3,
	};

	void RequestBehavior(SwordBehavior request);

	void IdleInit();
	void IdleUpdate(float deltaTime);


	Vec3 attack1RestPos = { -0.65f, 0.0f, 0.2f };
	Vec3 attack1RestRot = { 90.0f, 100.0f, 180.0f };
	Vec3 attack1EndPos = { 0.5f, 0.0f,1.3f };
	Vec3 attack1EndRot = { 90.0f, -80.0f, 180.0f };
	void Attack1Init();
	void Attack1Update(float deltaTime);

	Vec3 attack2RestPos = { -0.5f, 0.0f, 0.5f };
	Vec3 attack2RestRot = { 90.0f, 100.0f, 180.0f };
	Vec3 attack2EndPos = { 0.5f, 0.0f,0.75f };
	Vec3 attack2EndRot = { 90.0f, -80.0f, 180.0f };
	void Attack2Init();
	void Attack2Update(float deltaTime);


	Vec3 attack3swingPos = { 0.4f, -2.5f, -0.5f };
	Vec3 attack3swingRot = { 0.0f, 90.0f, 180.0f };
	Vec3 attack3swingEndPos = { 0.25f, 1.0f, -0.25f };
	Vec3 attack3swingEndRot = { -20.0f, 60.0f, 180.0f };
	void Attack3SwingInit();
	void Attack3SwingUpdate(float deltaTime);

	Vec3 attack3RestPos = { 0.0f, 1.0f, 0.0f };
	Vec3 attack3RestRot = { 90.0f, 130.0f, 90.0f };
	Vec3 attack3EndPos = { 0.0f, -3.0f,1.35f };
	Vec3 attack3EndRot = { 90.0f, 10.0f, 90.0f };
	float strikeTime_ = 0.75f;
	float strikeTimer_ = 0.0f;
	void Attack3Init();
	void Attack3Update(float deltaTime);

	void BehaviorInit();
	void BehaviorUpdate(float deltaTime);

	SwordBehavior behavior_ = SwordBehavior::Idle;
	std::optional<SwordBehavior> behaviorRequest_ = std::nullopt;


};
