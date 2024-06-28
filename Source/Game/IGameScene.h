#pragma once

#include "../Renderer/Renderer.h"
#include "../Engine/Renderer/D3D12.h"

class IGameScene {
public:
	virtual ~IGameScene() = default;
	virtual void Startup(D3D12* renderer, Window* window) = 0;
	virtual void Update() = 0;
	virtual void Render() = 0;
	virtual void Shutdown() = 0;
};