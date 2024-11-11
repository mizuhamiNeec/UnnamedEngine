#pragma once

#include "../Window/Window.h"

class Renderer {
public:
	virtual ~Renderer() = default;

	virtual void Init(Window* window) = 0;
	virtual void PreRender() = 0;
	virtual void PostRender() = 0;

protected:
	Window* window_ = nullptr;
};
