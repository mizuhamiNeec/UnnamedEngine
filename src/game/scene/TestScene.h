#pragma once

#include <memory>

#include <engine/Sprite/Sprite.h>

#include "base/BaseScene.h"

class GameScene;
class ReplayPlayerInputController;

class TestScene : public BaseScene {
public:
	void Init() override;
	void Update(float deltaTime) override;
	void Render() override;
	void Shutdown() override;

private:
	[[nodiscard]] bool HandleStartInput() const;
	[[nodiscard]] bool HandleStartViaUiClick() const;
	void               ResetBackgroundDemoScene();
	void               InitTitleSprites();
	void               UpdateTitleSprites(float deltaTime);
	void               UpdateTitleSpriteLayout();
	void               ShutdownTitleSprites();

	std::unique_ptr<GameScene>                   mBackgroundScene;
	std::shared_ptr<ReplayPlayerInputController> mReplayInputController;
	std::unique_ptr<Sprite>                      mTitleBackdropSprite;
	std::unique_ptr<Sprite>                      mTitleLogoSprite;
	std::unique_ptr<Sprite>                      mPressStartSprite;
	Vec3                                         mTitleLogoBaseSize  = Vec3::one;
	Vec3                                         mPressStartBaseSize = Vec3::one;
	float                                        mPromptBlinkTimeSec = 0.0f;
	float                                        mTickAccumulatorSec = 0.0f;
	bool                                         mPendingStartGame   = false;
};
