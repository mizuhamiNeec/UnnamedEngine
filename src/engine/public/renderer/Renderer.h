#pragma once

#include "../Window/MainWindow.h"

class Renderer {
public:
	virtual ~Renderer() = default;

	virtual void Init() = 0;
	virtual void Shutdown() = 0;
	virtual void PreRender() = 0;
	virtual void PostRender() = 0;
};
