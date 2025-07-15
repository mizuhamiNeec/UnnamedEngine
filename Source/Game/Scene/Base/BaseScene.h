#pragma once

#include <Entity/EntityLoader.h>
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
	virtual      ~BaseScene() = default;
	virtual void Init() = 0;
	virtual void Update(float deltaTime) = 0; // Entity, Componentの更新
	virtual void Render() = 0;                // Scene, Componentの描画
	virtual void Shutdown() = 0;              // シーンの終了処理

	virtual std::vector<Entity*>& GetEntities();
	virtual void                  AddEntity(Entity* entity);

	virtual void SetEditorMode(const bool isEditorMode) {
		mIsEditorMode = isEditorMode;
	}

	[[nodiscard]] virtual bool IsEditorMode() const { return mIsEditorMode; }
	void         RemoveEntity(Entity* entity);
	Entity*      CreateEntity(const std::string& value);

protected:
	std::vector<Entity*> mEntities; // シーンに存在するエンティティ

	bool mIsEditorMode = false; // エディターモードか?

	ResourceManager* mResourceManager = nullptr;

	SpriteCommon*    mSpriteCommon    = nullptr;
	ParticleManager* mParticleManager = nullptr;
	Object3DCommon*  mObject3DCommon  = nullptr;
	ModelCommon*     mModelCommon     = nullptr;
	SrvManager*      mSrvManager      = nullptr;
	EngineTimer*     mTimer           = nullptr;
};
