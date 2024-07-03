#pragma once
#include "IGameScene.h"

#include "../Renderer/Renderer.h"
#include "../Engine/Lib/Transform/Transform.h"

class GameScene : IGameScene {
public:
	void Init(D3D12* renderer, Window* window) override;
	void Update() override;
	void Render() override;
	void Shutdown() override;

private:
	Window* window_;
	D3D12* renderer_;

	Transform transform;

	Transform cameraTransform;

};

