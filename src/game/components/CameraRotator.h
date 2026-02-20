#pragma once
#include <engine/Components/base/Component.h>

class SceneComponent;

/// @brief カメラを回転させるコンポーネント
/// @details マウス入力に基づいてカメラの向きを制御し、ピッチとヨーの回転を管理します
class CameraRotator : public Component {
public:
	/// @brief デストラクタ
	~CameraRotator() override;

	// --- Component ---

	/// @brief エンティティにアタッチされた際の初期化処理
	/// @param owner 所有者エンティティ
	/// @details トランスフォームを取得し、初期回転値とコンソール変数を設定します 
	void OnAttach(Entity& owner) override;

	/// @brief 物理演算前の更新処理
	/// @param deltaTime 前フレームからの経過時間
	/// @details プレイヤーは物理演算の前にカメラの回転を更新する必要があるため、PrePhysicsで処理します
	void PrePhysics(float deltaTime) override;

	void Update(float deltaTime) override;
	
	/// @brief ImGuiインスペクタ用のUI描画
	void DrawInspectorImGui() override;

	// --- CameraRotator ---

	/// @brief アニメーションによるピッチオフセットを設定する
	/// @param pitch ピッチオフセット（度数法）
	void SetAnimationPitchOffset(float pitch) { mAnimationPitchOffset = pitch; }


	/// @brief アニメーションによるロールオフセットを設定する
	/// @param roll ロールオフセット（度数法）
	void SetAnimationRollOffset(float roll) { mAnimationRollOffset = roll; }

private:
	float mPitch = 0.0f;
	float mYaw   = 0.0f;

	// アニメーションオフセット(リコイルや揺れなど)
	float mAnimationPitchOffset = 0.0f; // degrees
	float mAnimationRollOffset  = 0.0f; // degrees
};
