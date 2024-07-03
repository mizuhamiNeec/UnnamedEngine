#pragma once

#include "../Window/Window.h"

class Renderer {
public:
	virtual ~Renderer() = default;

	virtual void Init(Window* window) = 0;
	virtual void PreRender() = 0;
	virtual void PostRender() = 0;
	virtual void Shutdown() = 0;
	virtual void ToggleFullscreen() = 0;
	virtual void OnSizeChanged(UINT width, UINT height, bool isMinimized) = 0;

protected:
	Window* window_ = nullptr;
	WindowConfig windowConfig_ = {};
};
