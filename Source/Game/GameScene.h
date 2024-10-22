#pragma once
#include "IGameScene.h"

#include "../Renderer/Renderer.h"
#include "../Engine/Lib/Transform/Transform.h"
#include "../Sprite/SpriteCommon.h"

class Sprite;

class GameScene : IGameScene {
public:
	void Init(D3D12* renderer, Window* window) override;
	void Update() override;
	void Render() override;
	void Shutdown() override;

private:
	Window* window_ = nullptr;
	D3D12* renderer_ = nullptr;

	std::unique_ptr<SpriteCommon> spriteCommon_;
	std::vector<Sprite*> sprites_;

	Transform transform;

	Transform cameraTransform;

	float fov = 90.0f; // deg
};
