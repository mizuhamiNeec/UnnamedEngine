#pragma once
#include <memory>

#include "Scene/Base/BaseScene.h"

#include "SceneManager/SceneManager.h"

class Editor {
public:
	explicit Editor(SceneManager& sceneManager);

private:
	void Init();

public:
	void Update(float deltaTime);
	void Render() const;

	static bool IsManipulating() {
		return bIsManipulating_;
	}

private:
	void ShowDockSpace();

	static void DrawGrid(
		float gridSize, float range, const Vec4& color, const Vec4& majorColor,
		const Vec4& axisColor, const Vec4& minorColor,
		const Vec3& cameraPosition, float drawRadius
	);

	static float RoundToNearestPowerOfTwo(float value);

private:
	SceneManager& sceneManager_; // シーンマネージャ

	std::shared_ptr<BaseScene> scene_;          // 現在編集中のシーン
	Entity*                    selectedEntity_; // 選択中のエンティティ

	// エディターのカメラ
	std::unique_ptr<Entity>          cameraEntity_;
	std::shared_ptr<CameraComponent> camera_;

	float gridSize_  = 64.0f;
	float gridRange_ = 16384.0f;

	static bool bIsManipulating_;
};
