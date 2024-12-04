// #pragma once
// #include <ostream>
//
// class Entity;
//
// class IComponent {
// public:
// 	explicit IComponent(Entity* entity) : parentEntity_(entity) {}
//
// 	virtual ~IComponent() = default;
//
// 	[[nodiscard]] Entity* ParentEntity() const { return parentEntity_; }
//
// 	virtual void Initialize() = 0;
// 	virtual void Update(float deltaTime) = 0;
// 	virtual void Terminate() = 0;
//
// 	virtual void Serialize(std::ostream& out) const = 0;
// 	virtual void Deserialize(std::istream& in) = 0;
//
// 	virtual void ImGuiDraw();
//
// protected:
// 	// このコンポーネントが引っ付いているエンティティのポインタ
// 	Entity* parentEntity_ = nullptr;
// };
