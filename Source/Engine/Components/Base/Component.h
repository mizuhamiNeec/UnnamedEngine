#pragma once

#include <d3d12.h>

class SceneComponent;
class Entity;

//-----------------------------------------------------------------------------
// エンティティにアタッチすることができるコンポーネントの基底クラスです
//-----------------------------------------------------------------------------
class Component {
public:
	Component();
	virtual ~Component();

	virtual void OnAttach(Entity& owner);
	virtual void OnDetach();

	virtual void PrePhysics(float deltaTime);
	virtual void Update(float deltaTime) = 0;
	virtual void PostPhysics(float deltaTime);

	virtual void Render(ID3D12GraphicsCommandList* commandList);

	virtual void DrawInspectorImGui() = 0;

	// エディター専用のコンポーネントを作成する場合はこの関数をオーバーライドします
	[[nodiscard]] virtual bool IsEditorOnly() const;

	[[nodiscard]] virtual Entity* GetOwner() const;

protected:
	Entity*             mOwner     = nullptr;
	SceneComponent* mTransform = nullptr;
};
