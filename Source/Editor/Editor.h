#pragma once
#include <memory>

#include "Scene/Base/BaseScene.h"

#include "SceneManager/SceneManager.h"

class Editor {
public:
	explicit Editor(SceneManager* sceneManager);
	void     Init();

public:
	void Update(float deltaTime);
	void Render() const;

	static bool IsManipulating() {
		return mIsManipulating;
	}

private:
	void ShowDockSpace();

	static void DrawGrid(
		float gridSize, float range, const Vec4& color, const Vec4& majorColor,
		const Vec4& axisColor, const Vec4& minorColor,
		const Vec3& cameraPosition, float drawRadius
	);

	static float RoundToNearestPowerOfTwo(float value);

	SceneManager* mSceneManager; // 持ってきたやつ

	std::shared_ptr<BaseScene> mScene;          // 現在編集中のシーン
	Entity*                    mSelectedEntity; // 選択中のエンティティ

	// エディターのカメラ
	std::unique_ptr<Entity>          mCameraEntity;
	std::shared_ptr<CameraComponent> mCamera;

	float mGridSize  = 64.0f;
	float mGridRange = 16384.0f;

	float mAngleSnap = 15.0f;

	static bool mIsManipulating;
};
