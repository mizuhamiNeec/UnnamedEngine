#pragma once
#include <memory>

#include <engine/public/CubeMap/CubeMap.h>
#include <engine/public/uphysics/UPhysics.h>

#include <game/public/scene/base/BaseScene.h>

class D3D12;

class EmptyScene : public BaseScene {
public:
	~EmptyScene() override;
	void Init() override;
	void Update(float deltaTime) override;
	void Render() override;
	void Shutdown() override;

private:
	D3D12*                   mRenderer = nullptr;
	std::unique_ptr<CubeMap> mCubeMap;

	std::unique_ptr<UPhysics::Engine> mPhysicsEngine;

	std::unique_ptr<Entity> mMeshEntity;
};
