#pragma once

class Entity;

class Component {
public:
	virtual ~Component();
	virtual void OnAttach(Entity& owner);
	virtual void Update(float deltaTime) = 0;

	// エディター専用のコンポーネントを作成する場合はこの関数をオーバーライドします
	[[nodiscard]] virtual bool IsEditorOnly() const { return false; }
private:
	Entity* owner_ = nullptr;
};
