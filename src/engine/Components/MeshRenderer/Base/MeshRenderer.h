#pragma once
#include <engine/Components/base/Component.h>

/// @brief メッシュレンダラーの基底クラス
class MeshRenderer : public Component {
public:
	MeshRenderer()           = default;
	~MeshRenderer() override = default;

	// 更新処理: 必要に応じてオーバーライドしてください
	void Update([[maybe_unused]] float deltaTime) override {
	}

	// 描画処理: オーバーライドしやがれください
	void Render(ID3D12GraphicsCommandList* commandList) override = 0;

	// インスペクターの描画: オーバーライドしやがれください
	void DrawInspectorImGui() override = 0;

protected:
	virtual void BindTransform(ID3D12GraphicsCommandList* commandList) = 0;
};
