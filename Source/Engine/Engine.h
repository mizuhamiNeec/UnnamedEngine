#pragma once
#include <memory>

#include "../Game/GameScene.h"

#include "../Renderer/SrvManager.h"

#include "Input/Input.h"
#include "Lib/Math/Quaternion/Quaternion.h"

#include "Lib/Timer/EngineTimer.h"
#include "Line/LineCommon.h"

#include "Model/ModelCommon.h"

#include "Particle/ParticleCommon.h"

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
	void Update() const;
	void Shutdown() const;

	static void RegisterConsoleCommandsAndVariables();
	static void Quit(const std::vector<std::string>& args = {});

	static void DrawGrid(
		float gridSize, float range, const Vec4& color, const Vec4& majorColor, const Vec4& axisColor, const Vec4&
		minorColor
	);

private:
	Input* input_ = nullptr;

	std::unique_ptr<Window> window_;
	std::unique_ptr<D3D12> renderer_;
	std::unique_ptr<SrvManager> srvManager_;
	std::unique_ptr<EngineTimer> time_;
	std::unique_ptr<GameScene> gameScene_;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<SpriteCommon> spriteCommon_;
	std::unique_ptr<Object3DCommon> object3DCommon_;
	std::unique_ptr<ParticleCommon> particleCommon_;
	std::unique_ptr<ModelCommon> modelCommon_;
	std::unique_ptr<LineCommon> lineCommon_;

private:
	static bool bWishShutdown;

#ifdef _DEBUG
	std::unique_ptr<ImGuiManager> imGuiManager_;
	std::unique_ptr<Console> console_;
#endif
};
