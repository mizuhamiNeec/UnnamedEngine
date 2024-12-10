#pragma once
#include <ostream>

class BaseEntity;

class BaseComponent {
public:
	explicit BaseComponent(BaseEntity* entity) : parentEntity_(entity) {
	}

	virtual ~BaseComponent() = default;

	[[nodiscard]] BaseEntity* ParentEntity() const { return parentEntity_; }

	virtual void Initialize() = 0;
	virtual void Update(float deltaTime) = 0;
	virtual void Terminate() = 0;

	virtual void Serialize(std::ostream& out) const = 0;
	virtual void Deserialize(std::istream& in) = 0;

	virtual void ImGuiDraw() = 0;

protected:
	// このコンポーネントが引っ付いているエンティティのポインタ
	BaseEntity* parentEntity_ = nullptr;
};
