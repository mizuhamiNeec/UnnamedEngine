#include <engine/public/Components/Base/Component.h>

#include <engine/public/Entity/Entity.h>

Component::Component() = default;

Component::~Component() {
}

//-----------------------------------------------------------------------------
// Purpose: エンティティにアタッチされたときに呼び出されます
// 基底ではオーナーを設定します
//-----------------------------------------------------------------------------
void Component::OnAttach(Entity& owner) {
	this->mOwner = &owner;
	this->mScene = owner.GetTransform();
}

//-----------------------------------------------------------------------------
// Purpose: エンティティからデタッチされるときに呼び出されます
// 基底ではオーナーをnullptrに設定します
//-----------------------------------------------------------------------------
void Component::OnDetach() {
	this->mOwner = nullptr;
}

void Component::PrePhysics([[maybe_unused]] float deltaTime) {
}

void Component::PostPhysics([[maybe_unused]] float deltaTime) {
}

//-----------------------------------------------------------------------------
// Purpose: 描画処理のあるコンポーネントはこの関数をオーバーライドします
//-----------------------------------------------------------------------------
void Component::Render(
	[[maybe_unused]] ID3D12GraphicsCommandList* commandList) {
}

//-----------------------------------------------------------------------------
// Purpose: エディター専用のコンポーネントを作成する場合はこの関数をオーバーライドします
//----------------------------------------------------------------------------- 
bool Component::IsEditorOnly() const {
	return false;
}

Entity* Component::GetOwner() const {
	return mOwner;
}
