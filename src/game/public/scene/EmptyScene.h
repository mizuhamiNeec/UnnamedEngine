#pragma once
#include <memory>

#include <game/public/scene/base/BaseScene.h>

#include <engine/public/CubeMap/CubeMap.h>
#include <engine/public/Entity/Entity.h>

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
};
