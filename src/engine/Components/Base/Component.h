#pragma once

#include <d3d12.h>

class SceneComponent;
class Entity;


/// @brief エンティティにアタッチ可能なコンポーネントの基底クラス
/// @details 全てのコンポーネントが継承すべき基本機能を提供します。
///         更新、物理演算、描画のライフサイクルを定義します。
class Component {
public:
	Component();

	virtual      ~Component();
	virtual void OnAttach(Entity& owner);
	virtual void OnDetach();

	virtual void PrePhysics(float deltaTime);

	/// @brief 毎フレームの更新処理（純粋仮想関数）
	/// @param deltaTime 前フレームからの経過時間
	virtual void Update(float deltaTime) = 0;

	virtual void PostPhysics(float deltaTime);

	virtual void Render(ID3D12GraphicsCommandList* commandList);

	/// @brief ImGuiインスペクタ用のUI描画（純粋仮想関数）
	virtual void DrawInspectorImGui() = 0;

	[[nodiscard]] virtual bool    IsEditorOnly() const;
	[[nodiscard]] virtual Entity* GetOwner() const;

protected:
	Entity*         mOwner = nullptr;
	SceneComponent* mScene = nullptr;
};
