#pragma once
#include "Input.h"
#include "SpriteCommon.h"
#include "Source/Engine/ImGuiManager/ImGuiManager.h"
#include "Source/Engine/Renderer/D3D12.h"
#include "Source/Game/GameScene.h"

class Engine {
public:
	void Run();

private:
	void Initialize();
	void Update() const;
	void Shutdown();

private:
	std::unique_ptr<Window> window_;
	std::unique_ptr<D3D12> renderer_;
	std::unique_ptr<SpriteCommon> spriteCommon_;
	std::unique_ptr<Input> input_;
	std::unique_ptr<GameScene> gameScene_;

	// Dev
	std::unique_ptr<ImGuiManager> imGuiManager_;
	std::unique_ptr<Console> console_;
};

