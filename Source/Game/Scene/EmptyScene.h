#pragma once
#include <memory>
#include <Scene/Base/BaseScene.h>
#include <CubeMap/CubeMap.h>
#include <Renderer/Renderer.h>

class EmptyScene : public BaseScene {
public:
	~EmptyScene() override;
	void Init() override;
	void Update(float deltaTime) override;
	void Render() override;
	void Shutdown() override;

private:
	D3D12*                   renderer_ = nullptr;
	std::unique_ptr<CubeMap> cubeMap_;

	std::unique_ptr<Entity> skeletalMeshEntity_;
};
