#pragma once
#include <memory>

#include <engine/Components/Camera/CameraComponent.h>
#include <engine/Entity/Entity.h>
#include <engine/unnamed/subsystem/time/GameTime.h>

#include "game/scene/base/BaseScene.h"

class GameTime;
class SceneManager;
class EntityLoader;

/// @brief エディタークラス
class Editor {
public:
	Editor(SceneManager* sceneManager, GameTime* gameTime);
	~Editor();
	void Init();

	void DrawMenuBars();
	void Update(float deltaTime);
	void Render() const;

	void SetEntityLoader(EntityLoader* entityLoader);

	static bool IsManipulating();

private:
	void ActivateEditorCamera() const;

#ifdef _DEBUG
	void DrawInspector() const;
	void DrawOutliner();
	void DrawMainMenuBar();
	void DrawTopBar();
	void DrawSideBar();
	void DrawStatusBar();
#endif

	static void DrawGrid(
		float gridSize, float range, const Vec4& color, const Vec4& majorColor,
		const Vec4& axisColor, const Vec4& minorColor,
		const Vec3& cameraPosition, float drawRadius
	);

	static float RoundToNearestPowerOfTwo(float value);

	// 持ってきたやつ
	SceneManager* mSceneManager = nullptr;
	GameTime*     mGameTime     = nullptr;
	EntityLoader* mEntityLoader = nullptr;

	std::optional<std::string> mLoadFilePath;

	std::shared_ptr<BaseScene> mScene;                    // 現在編集中のシーン
	Entity*                    mSelectedEntity = nullptr; // 選択中のエンティティ

	// エディターのカメラ
	std::unique_ptr<Entity>          mCameraEntity;
	std::shared_ptr<CameraComponent> mCamera;
	Entity*                          mCameraEntityRaw;

	float mGridSize  = 64.0f;
	float mGridRange = 16384.0f;

	float mAngleSnap = 15.0f;

	static bool mIsManipulating;
};
