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
	void Render() const;

private:
	static void DrawGrid(
		float gridSize, float range, const Vec4& color, const Vec4& majorColor,
		const Vec4& axisColor, const Vec4& minorColor, const Vec3& cameraPosition, float drawRadius
	);

private:
	std::shared_ptr<Scene> scene_; // 現在編集中のシーン
	Entity* selectedEntity_; // 選択中のエンティティ

	// エディターのカメラ
	std::unique_ptr<Entity> cameraEntity_;
	std::shared_ptr<CameraComponent> camera_;

	float gridSize_ = 64.0f;
	float gridRange_ = 16384.0f;
};
