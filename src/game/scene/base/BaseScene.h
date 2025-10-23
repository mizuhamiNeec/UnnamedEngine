#pragma once

#include <vector>
#include <engine/Entity/Entity.h>

class SrvManager;
class ModelCommon;
class Object3DCommon;
class ParticleManager;
class SpriteCommon;
class ResourceManager;
class EngineTimer;

/**
 * @brief シーンの基底クラス
 * @details 全てのシーンが継承すべき基本機能を提供します。
 *          エンティティの管理、更新、描画のライフサイクルを定義します。
 */
class BaseScene {
public:
	/**
	 * @brief 仮想デストラクタ
	 */
	virtual ~BaseScene() = default;

	/**
	 * @brief シーンの初期化処理
	 */
	virtual void Init() = 0;

	/**
	 * @brief エンティティとコンポーネントの更新処理
	 * @param deltaTime 前フレームからの経過時間
	 */
	virtual void Update(float deltaTime) = 0;

	/**
	 * @brief シーンとコンポーネントの描画処理
	 */
	virtual void Render() = 0;

	/**
	 * @brief シーンの終了処理
	 */
	virtual void Shutdown() = 0;

	/**
	 * @brief シーン内のエンティティリストを取得する
	 * @return エンティティリストへの参照
	 */
	virtual std::vector<Entity*>& GetEntities();

	/**
	 * @brief シーンにエンティティを追加する
	 * @param entity 追加するエンティティ
	 */
	virtual void AddEntity(Entity* entity);

	/**
	 * @brief エディターモードを設定する
	 * @param isEditorMode エディターモードにするか
	 */
	virtual void SetEditorMode(const bool isEditorMode) {
		mIsEditorMode = isEditorMode;
	}

	/**
	 * @brief エディターモードかどうかを取得する
	 * @return エディターモードの場合true
	 */
	[[nodiscard]] virtual bool IsEditorMode() const { return mIsEditorMode; }

	/**
	 * @brief シーンからエンティティを削除する
	 * @param entity 削除するエンティティ
	 */
	void RemoveEntity(Entity* entity);

	/**
	 * @brief 新しいエンティティを作成する
	 * @param value エンティティ名
	 * @return 作成されたエンティティへのポインタ
	 */
	Entity* CreateEntity(const std::string& value);

protected:
	std::vector<Entity*> mEntities; // シーンに存在するエンティティ

	bool mIsEditorMode = false; // エディターモードか?

	ResourceManager* mResourceManager = nullptr;

	SpriteCommon*    mSpriteCommon    = nullptr;
	ParticleManager* mParticleManager = nullptr;
	Object3DCommon*  mObject3DCommon  = nullptr;
	ModelCommon*     mModelCommon     = nullptr;
	SrvManager*      mSrvManager      = nullptr;
	EngineTimer*     mTimer           = nullptr;
};
