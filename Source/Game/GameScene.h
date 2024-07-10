#pragma once
#include "IGameScene.h"

#include "../../Shared/Transform/Transform.h"

class GameScene : IGameScene {
public:
	void Startup(D3D12* renderer, Window* window) override;
	void Update() override;
	void Render() override;
	void Shutdown() override;

private:
	Window* window_;
	D3D12* renderer_;

	Transform transform;

	Transform cameraTransform;

};

