#pragma once

#include "../Engine/Renderer/D3D12.h"
#include "../Object3D/Object3DCommon.h"
#include "../Renderer/Renderer.h"
#include "../Sprite/SpriteCommon.h"

class IGameScene {
public:
	virtual ~IGameScene() = default;
	virtual void Init(D3D12* renderer, Window* window, SpriteCommon* spriteCommon, Object3DCommon* object3DCommon) = 0;
	virtual void Update() = 0;
	virtual void Render() = 0;
	virtual void Shutdown() = 0;

protected:
	SpriteCommon* spriteCommon_ = nullptr;
	Object3DCommon* object3DCommon_ = nullptr;
};
