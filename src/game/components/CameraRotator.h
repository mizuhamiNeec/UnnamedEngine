#pragma once
#include <engine/Components/base/Component.h>

class SceneComponent;

/**
 * @brief カメラを回転させるコンポーネント
 * @details マウス入力に基づいてカメラの向きを制御し、ピッチとヨーの回転を管理します
 */
class CameraRotator : public Component {
public:
	/**
	 * @brief デストラクタ
	 */
	~CameraRotator() override;

	/**
	 * @brief エンティティにアタッチされた際に呼ばれる
	 * @param owner 所有者エンティティ
	 */
	void OnAttach(Entity& owner) override;

	/**
	 * @brief 毎フレーム更新処理を行う
	 * @param deltaTime 前フレームからの経過時間
	 */
	void Update(float deltaTime) override;

	/**
	 * @brief ImGuiインスペクタ用のUI描画
	 */
	void DrawInspectorImGui() override;

private:
	float mPitch = 0.0f;
	float mYaw   = 0.0f;

	bool isMouseLocked_ = true;

	// アニメーションオフセット(リコイルや揺れなど)
	float mAnimationPitchOffset = 0.0f; // degrees
	float mAnimationRollOffset  = 0.0f; // degrees

public:
	/**
	 * @brief アニメーションによるピッチオフセットを設定する
	 * @param pitch ピッチオフセット（度数法）
	 */
	void SetAnimationPitchOffset(float pitch) { mAnimationPitchOffset = pitch; }

	/**
	 * @brief アニメーションによるロールオフセットを設定する
	 * @param roll ロールオフセット（度数法）
	 */
	void SetAnimationRollOffset(float roll) { mAnimationRollOffset = roll; }
};
