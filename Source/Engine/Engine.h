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

	static void DrawLine(const Vec3& start, const Vec3& end, const Vec4& color);
	static void DrawRay(const Vec3& position, const Vec3& dir, const Vec4& color);
	static void DrawAxis(const Vec3& position, const Quaternion& orientation);
	static void DrawCircle(
		const Vec3& position, const Quaternion& rotation, const float& radius,
		const Vec4& color, const uint32_t& segments = 32
	);
	static void DrawArc(
		const float& startAngle, const float& endAngle, const Vec3& position,
		const Quaternion& orientation, const float& radius, const Vec4& color,
		const bool& drawChord = false, const bool& drawSector = false, const int& arcSegments = 32
	);
	static void DrawArrow(const Vec3& position, const Vec3& direction, const Vec4& color, float headSize);
	static void DrawQuad(
		const Vec3& pointA, const Vec3& pointB, const Vec3& pointC, const Vec3& pointD, const Vec4& color
	);
	static void DrawRect(const Vec3& position, const Quaternion& orientation, const Vec2& extent, const Vec4& color);
	static void DrawRect(
		const Vec2& point1, const Vec2& point2, const Vec3& origin, const Quaternion& orientation, const Vec4& color
	);
	static void DrawSphere(
		const Vec3& position, const Quaternion& orientation, float& radius, const Vec4& color, int segments
	);
	static void DrawBox(const Vec3& position, const Quaternion& orientation, Vec3& size, const Vec4& color);
	static void DrawCylinder(
		const Vec3& position, const Quaternion& orientation, const float& height,
		const float& radius, const Vec4& color, const bool& drawFromBase = true
	);
	static void DrawCapsule(
		const Vec3& position, const Quaternion& orientation, const float& height,
		float& radius, const Vec4& color, const bool& drawFromBase = true
	);
	static void DrawGrid(
		float gridSize, float range, const Vec4& color, const Vec4& majorColor, const Vec4& axisColor, const Vec4&
		minorColor
	);

private:
	void Init();
	void Update() const;
	void Shutdown() const;

	static void RegisterConsoleCommandsAndVariables();
	static void Quit(const std::vector<std::string>& args = {});

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

	static std::unique_ptr<Line> line_;

#ifdef _DEBUG
	std::unique_ptr<ImGuiManager> imGuiManager_;
	std::unique_ptr<Console> console_;
#endif
};

static bool bWishShutdown = false;
