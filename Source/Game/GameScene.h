#pragma once
#include <memory>

#include "IGameScene.h"
#include "../Object3D/Object3D.h"
#include "../Object3D/Object3DCommon.h"
#include "../Renderer/Renderer.h"
#include "../Sprite/Sprite.h"
#include "../Sprite/SpriteCommon.h"

class GameScene : IGameScene {
public:
	void Init(D3D12* renderer, Window* window, SpriteCommon* spriteCommon, Object3DCommon* object3DCommon) override;
	void Update() override;
	void Render() override;
	void Shutdown() override;

private:
	Window* window_ = nullptr;
	D3D12* renderer_ = nullptr;

	std::vector<Sprite*> sprites_;
	std::unique_ptr<Object3D> object3D_;

	Transform transform_;
	Transform cameraTransform_;

	float fov_ = 90.0f; // deg
};
