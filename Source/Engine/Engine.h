#pragma once
#include <memory>

#include "../Game/GameScene.h"

#include "Model/ModelCommon.h"

#include "Timer/EngineTimer.h"

class Console;
class ImGuiManager;
class D3D12;
class Window;

class Engine {
public:
	Engine();
	void Run();

private:
	void Init();
	void Update();
	void Shutdown() const;

private:
	std::unique_ptr<Window> window_;
	std::unique_ptr<D3D12> renderer_;
	std::unique_ptr<EngineTimer> time_;
	std::unique_ptr<GameScene> gameScene_;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<SpriteCommon> spriteCommon_;
	std::unique_ptr<Object3DCommon> object3DCommon_;
	std::unique_ptr<ModelCommon> modelCommon_;

#ifdef _DEBUG
	std::unique_ptr<ImGuiManager> imGuiManager_;
	std::unique_ptr<Console> console_;
#endif
};
