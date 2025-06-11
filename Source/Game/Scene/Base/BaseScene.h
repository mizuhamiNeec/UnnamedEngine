#pragma once

#include <Entity/Base/Entity.h>
#include <Model/ModelCommon.h>
#include <Object3D/Object3DCommon.h>
#include <Particle/ParticleManager.h>
#include <ResourceSystem/Manager/ResourceManager.h>
#include <Sprite/SpriteCommon.h>

class Engine;
class EngineTimer;

class BaseScene {
public:
	virtual ~BaseScene() = default;
	virtual void Init() = 0;
	virtual void Update(float deltaTime) = 0; // Entity, Componentの更新
	virtual void Render() = 0; // Scene, Componentの描画
	virtual void Shutdown() = 0; // シーンの終了処理

	virtual std::vector<Entity*>& GetEntities();
	virtual void AddEntity(Entity* entity);

	virtual void SetEditorMode(bool isEditorMode) { isEditorMode_ = isEditorMode; }
	virtual bool IsEditorMode() const { return isEditorMode_; }
	void RemoveEntity(Entity* entity);

protected:
	std::vector<Entity*> entities_; // シーンに存在するエンティティ

	bool isEditorMode_ = false; // エディターモードか?

	ResourceManager* resourceManager_ = nullptr;

	SpriteCommon* spriteCommon_ = nullptr;
	ParticleManager* particleManager_ = nullptr;
	Object3DCommon* object3DCommon_ = nullptr;
	ModelCommon* modelCommon_ = nullptr;
	//SrvManager* srvManager_ = nullptr;
	EngineTimer* timer_ = nullptr;
};
