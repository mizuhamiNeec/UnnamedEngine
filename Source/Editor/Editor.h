#pragma once
#include <memory>

#include "Scene/Base/Scene.h"

class Editor {
public:
	explicit Editor(std::shared_ptr<Scene> scene);
private:
	void Init();
public:
	void Update(float deltaTime);
	void Render();

private:
	void DrawGrid(
		const float gridSize, const float range, const Vec4& color, const Vec4& majorColor,
		const Vec4& axisColor, const Vec4& minorColor
	);

private:
	std::shared_ptr<Scene> scene_; // 現在編集中のシーン
};
