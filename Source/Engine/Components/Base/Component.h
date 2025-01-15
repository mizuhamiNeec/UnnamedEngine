#pragma once

class Entity;

class Component {
public:
	virtual ~Component();

	// エンティティにアタッチされたときに呼び出されます
	virtual void OnAttach(Entity& owner);
	// コンポーネントの更新処理
	virtual void Update(float deltaTime) = 0;
	// インスペクターの描画
	virtual void DrawInspectorImGui() = 0;

	// エディター専用のコンポーネントを作成する場合はこの関数をオーバーライドします
	[[nodiscard]] virtual bool IsEditorOnly() const;

protected:
	Entity* owner_ = nullptr;
};
