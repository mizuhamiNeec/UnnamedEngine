#pragma once

#include <d3d12.h>

class SceneComponent;
class Entity;

/**
 * @brief エンティティにアタッチ可能なコンポーネントの基底クラス
 * @details 全てのコンポーネントが継承すべき基本機能を提供します。
 *          更新、物理演算、描画のライフサイクルを定義します。
 */
class Component {
public:
	/**
	 * @brief コンストラクタ
	 */
	Component();
	
	/**
	 * @brief 仮想デストラクタ
	 */
	virtual ~Component();

	/**
	 * @brief エンティティにアタッチされた際に呼ばれる
	 * @param owner 所有者エンティティ
	 */
	virtual void OnAttach(Entity& owner);
	
	/**
	 * @brief エンティティからデタッチされた際に呼ばれる
	 */
	virtual void OnDetach();

	/**
	 * @brief 物理演算前の処理
	 * @param deltaTime 前フレームからの経過時間
	 */
	virtual void PrePhysics(float deltaTime);
	
	/**
	 * @brief 毎フレームの更新処理（純粋仮想関数）
	 * @param deltaTime 前フレームからの経過時間
	 */
	virtual void Update(float deltaTime) = 0;
	
	/**
	 * @brief 物理演算後の処理
	 * @param deltaTime 前フレームからの経過時間
	 */
	virtual void PostPhysics(float deltaTime);

	/**
	 * @brief 描画処理
	 * @param commandList DirectX 12のコマンドリスト
	 */
	virtual void Render(ID3D12GraphicsCommandList* commandList);

	/**
	 * @brief ImGuiインスペクタ用のUI描画（純粋仮想関数）
	 */
	virtual void DrawInspectorImGui() = 0;

	/**
	 * @brief エディター専用コンポーネントかどうかを取得する
	 * @return エディター専用の場合true
	 * @details エディター専用のコンポーネントを作成する場合はこの関数をオーバーライドします
	 */
	[[nodiscard]] virtual bool IsEditorOnly() const;

	/**
	 * @brief 所有者エンティティを取得する
	 * @return 所有者エンティティへのポインタ
	 */
	[[nodiscard]] virtual Entity* GetOwner() const;

protected:
	Entity*             mOwner     = nullptr;
	SceneComponent* mScene = nullptr;
};
