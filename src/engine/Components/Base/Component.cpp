#include <engine/Components/Base/Component.h>

#include <engine/Entity/Entity.h>

/**
 * @brief デフォルトコンストラクタ
 */
Component::Component() = default;

/**
 * @brief デストラクタ
 */
Component::~Component() {
}

/**
 * @brief エンティティにアタッチされた際の処理
 * @param owner 所有者エンティティ
 * @details 所有者とトランスフォームへの参照を保持します
 */
void Component::OnAttach(Entity& owner) {
	this->mOwner = &owner;
	this->mScene = owner.GetTransform();
}

/**
 * @brief エンティティからデタッチされる際の処理
 * @details 所有者への参照をクリアします
 */
void Component::OnDetach() {
	this->mOwner = nullptr;
}

/**
 * @brief 物理演算前の処理
 * @param deltaTime 前フレームからの経過時間（未使用）
 * @details 必要に応じてオーバーライドします
 */
void Component::PrePhysics([[maybe_unused]] float deltaTime) {
}

/**
 * @brief 物理演算後の処理
 * @param deltaTime 前フレームからの経過時間（未使用）
 * @details 必要に応じてオーバーライドします
 */
void Component::PostPhysics([[maybe_unused]] float deltaTime) {
}

/**
 * @brief 描画処理
 * @param commandList DirectX 12のコマンドリスト（未使用）
 * @details 描画処理のあるコンポーネントはこの関数をオーバーライドします
 */
void Component::Render(
	[[maybe_unused]] ID3D12GraphicsCommandList* commandList) {
}

/**
 * @brief エディター専用コンポーネントかどうかを取得する
 * @return エディター専用の場合true（基底実装ではfalse）
 * @details エディター専用のコンポーネントを作成する場合はこの関数をオーバーライドします
 */
bool Component::IsEditorOnly() const {
	return false;
}

/**
 * @brief 所有者エンティティを取得する
 * @return 所有者エンティティへのポインタ
 */
Entity* Component::GetOwner() const {
	return mOwner;
}
