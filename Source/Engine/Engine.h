#pragma once
#include <memory>

#include "../Game/GameScene.h"

#include "../Input/Input.h"

class Console;
class ImGuiManager;
class D3D12;
class Window;

class Engine {
public:
	Engine();
	void Run();

private:
	void Initialize();
	void Update() const;
	void Shutdown() const;

private:
	std::unique_ptr<Window> window_;
	std::unique_ptr<D3D12> renderer_;
	std::unique_ptr<Input> input_;
	std::unique_ptr<GameScene> gameScene_;

	// Dev
	std::unique_ptr<ImGuiManager> imGuiManager_;
	std::unique_ptr<Console> console_;
};

