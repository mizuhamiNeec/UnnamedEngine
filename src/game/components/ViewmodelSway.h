#pragma once

#include <engine/Components/Base/Component.h>

/**
 * @brief 武器、手、その他を揺らすコンポーネント
 * @details カメラの動きに応じて武器モデルに揺れの演出を加えます
 */
class ViewmodelSway final : public Component {
public:
	/**
	 * @brief コンストラクタ
	 * @param swayAmount 揺れの量
	 */
	ViewmodelSway(const float swayAmount = -0.025f) : mSwayAmount(swayAmount) {}

	/**
	 * @brief デストラクタ
	 */
	~ViewmodelSway() override;

	/**
	 * @brief エンティティにアタッチされた際に呼ばれる
	 * @param owner 所有者エンティティ
	 */
	void OnAttach(Entity& owner) override;

	void PrePhysics(float deltaTime) override;
	
	/**
	 * @brief 毎フレーム更新処理を行う
	 * @param deltaTime 前フレームからの経過時間
	 */
	void Update(float deltaTime) override;

	/**
	 * @brief 描画処理を行う
	 * @param commandList DirectX 12のコマンドリスト
	 */
	void Render(ID3D12GraphicsCommandList* commandList) override;

	/**
	 * @brief ImGuiインスペクタ用のUI描画
	 */
	void DrawInspectorImGui() override;

	/**
	 * @brief 所有者エンティティを取得する
	 * @return 所有者エンティティへのポインタ
	 */
	[[nodiscard]] Entity* GetOwner() const override;

private:
	float mSwayAmount  = 0.0f; // 揺らす量
	float mPitch       = 0.0f;
	float mYaw         = 0.0f;
	float mAttenuation = 16.0f;
};
