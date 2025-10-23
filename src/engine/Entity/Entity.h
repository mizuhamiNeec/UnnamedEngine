#pragma once
#include <algorithm>
#include <d3d12.h>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <engine/Components/Base/Component.h>
#include <engine/Components/Transform/SceneComponent.h>

enum class EntityType {
	RuntimeOnly,
	EditorOnly,
	Shared,
};

/**
 * @brief ゲームオブジェクトを表現するエンティティクラス
 * @details コンポーネントベースのアーキテクチャを採用し、
 *          機能をコンポーネントとして追加・管理します。
 */
class Entity {
public:
	/**
	 * @brief コンストラクタ
	 * @param name エンティティ名
	 * @param type エンティティの種類（デフォルト: RuntimeOnly）
	 */
	explicit Entity(std::string       name,
	                const EntityType& type = EntityType::RuntimeOnly) :
		mScene(std::make_unique<SceneComponent>()),
		mEntityType(type),
		mName(std::move(name)) {
		mScene->OnAttach(*this);
	}

	/**
	 * @brief デストラクタ
	 */
	~Entity();

	/**
	 * @brief 物理演算前の処理
	 * @param deltaTime 前フレームからの経過時間
	 */
	void PrePhysics(float deltaTime) const;

	/**
	 * @brief 毎フレームの更新処理
	 * @param deltaTime 前フレームからの経過時間
	 */
	void Update(float deltaTime);

	/**
	 * @brief 物理演算後の処理
	 * @param deltaTime 前フレームからの経過時間
	 */
	void PostPhysics(float deltaTime) const;

	/**
	 * @brief 描画処理
	 * @param commandList DirectX 12のコマンドリスト
	 */
	void Render(ID3D12GraphicsCommandList* commandList) const;

	/**
	 * @brief エンティティの種類を取得する
	 * @return エンティティの種類
	 */
	[[nodiscard]] EntityType GetType() const;

	/**
	 * @brief エンティティの種類を設定する
	 * @param type エンティティの種類
	 */
	void SetType(const EntityType& type);

	/**
	 * @brief トランスフォームコンポーネントを取得する
	 * @return トランスフォームコンポーネントへのポインタ
	 */
	[[nodiscard]] SceneComponent* GetTransform() const;

	/**
	 * @brief エンティティがアクティブかどうかを取得する
	 * @return アクティブの場合true
	 */
	[[nodiscard]] bool IsActive() const;

	/**
	 * @brief エンティティのアクティブ状態を設定する
	 * @param active アクティブにする場合true
	 */
	void SetActive(bool active);

	/**
	 * @brief エンティティが可視かどうかを取得する
	 * @return 可視の場合true
	 */
	[[nodiscard]] bool IsVisible() const;

	/**
	 * @brief エンティティの可視状態を設定する
	 * @param visible 可視にする場合true
	 */
	void SetVisible(bool visible);

	/**
	 * @brief コンポーネントを追加する
	 * @tparam T コンポーネントの型
	 * @tparam Args コンストラクタ引数の型
	 * @param args コンストラクタ引数
	 * @return 追加されたコンポーネントへのポインタ
	 */
	template <typename T, typename... Args>
	T* AddComponent(Args&&... args);

	/**
	 * @brief 指定された型のコンポーネントを取得する
	 * @tparam T コンポーネントの型
	 * @return コンポーネントへのポインタ（存在しない場合nullptr）
	 */
	template <typename T>
	T* GetComponent();

	/**
	 * @brief 指定された型のコンポーネントを持っているか判定する
	 * @tparam T コンポーネントの型
	 * @return コンポーネントを持っている場合true
	 */
	template <typename T>
	bool HasComponent() const;

	/**
	 * @brief 指定された型のコンポーネントを削除する
	 * @tparam T コンポーネントの型
	 * @return 削除に成功した場合true
	 */
	template <typename T>
	bool RemoveComponent();

	/**
	 * @brief 指定された型の全てのコンポーネントを取得する
	 * @tparam T コンポーネントの型
	 * @return コンポーネントのリスト
	 */
	template <typename T>
	std::vector<T*> GetComponents();

	/**
	 * @brief 親エンティティを設定する
	 * @param newParent 新しい親エンティティ
	 */
	void SetParent(Entity* newParent);

	/**
	 * @brief 親エンティティを取得する
	 * @return 親エンティティへのポインタ
	 */
	[[nodiscard]] Entity* GetParent() const;

	/**
	 * @brief 子エンティティのリストを取得する
	 * @return 子エンティティのリスト
	 */
	[[nodiscard]] const std::vector<Entity*>& GetChildren() const;

	/**
	 * @brief 子エンティティを追加する
	 * @param child 追加する子エンティティ
	 */
	void AddChild(Entity* child);

	/**
	 * @brief 子エンティティを削除する
	 * @param child 削除する子エンティティ
	 */
	void RemoveChild(Entity* child);

	/**
	 * @brief エンティティ名を取得する
	 * @return エンティティ名への参照
	 */
	std::string& GetName();

	/**
	 * @brief エンティティ名を設定する
	 * @param name 新しいエンティティ名
	 */
	void SetName(const std::string& name);

	/**
	 * @brief 全てのコンポーネントを削除する
	 */
	void RemoveAllComponents();

private:
	Entity*              mParent = nullptr;
	std::vector<Entity*> mChildren;

	std::unique_ptr<SceneComponent>         mScene;
	std::vector<std::unique_ptr<Component>> mComponents;
	EntityType                              mEntityType; // エンティティの種類
	std::string                             mName;       // エンティティの名前

	bool bIsActive_  = true; // Updateを呼ぶかどうか
	bool bIsVisible_ = true; // 描画を行うかどうか
};

template <typename T, typename... Args>
T* Entity::AddComponent(Args&&... args) {
	static_assert(std::is_base_of_v<Component, T>,
	              "T must derive from Component");
	auto component = std::make_unique<T>(std::forward<Args>(args)...);
	T*   rawPtr    = component.get();
	mComponents.emplace_back(std::move(component));
	rawPtr->OnAttach(*this);
	return rawPtr;
}

template <typename T>
T* Entity::GetComponent() {
	for (const auto& component : mComponents) {
		if (auto* castedComponent = dynamic_cast<T*>(component.get())) {
			return castedComponent;
		}
	}
	return nullptr;
}

template <typename T>
std::vector<T*> Entity::GetComponents() {
	static_assert(std::is_base_of_v<Component, T>,
	              "T must derive from Component");
	std::vector<T*> result;
	for (const auto& component : mComponents) {
		if (auto castedComponent = dynamic_cast<T*>(component.get())) {
			result.emplace_back(castedComponent);
		}
	}
	return result;
}

template <typename T>
bool Entity::HasComponent() const {
	static_assert(std::is_base_of_v<Component, T>,
	              "T must derive from Component");
	for (const auto& component : mComponents) {
		if (dynamic_cast<T*>(component.get())) {
			return true;
		}
	}
	return false;
}

template <typename T>
bool Entity::RemoveComponent() {
	static_assert(std::is_base_of_v<Component, T>,
	              "T must derive from Component");
	for (auto it = mComponents.begin(); it != mComponents.end(); ++it) {
		if (auto* castedComponent = dynamic_cast<T*>(it->get())) {
			// コンポーネントを削除する前にOnDetachを呼ぶ
			castedComponent->OnDetach();
			mComponents.erase(it);
			return true; // 最初に見つかったコンポーネントを削除
		}
	}
	return false; // 見つからなかった
}
