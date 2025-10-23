#pragma once
#include <memory>
#include <string>

#include <engine/Components/base/Component.h>

#include <runtime/core/math/Math.h>

class Entity;

/**
 * @brief 武器データ構造体
 * @details 武器の性能パラメータを保持します
 */
struct WeaponData {
	std::string name;
	int         maxAmmo    = 0;         // 最大弾薬数
	int         clipSize   = 0;         // クリップサイズ
	float       fireRate   = 0.0f;      // 発射レート [秒あたりの発射数]
	float       reloadTime = 0.0f;      // リロード時間 [秒]
	float       damage     = 0.0f;      // 攻撃力
	std::string primaryModule;          // モジュール名
	std::string secondaryModule;        // モジュール名
	float       projectileSpeed = 0.0f; // 弾丸の速度 [HU/s]

	/**
	 * @brief JSONファイルから武器データを読み込む
	 * @param jsonPath JSONファイルのパス
	 * @return 武器データのユニークポインタ
	 */
	static std::unique_ptr<WeaponData>
	LoadFromJson(const std::string& jsonPath);
};

/**
 * @brief 武器モジュールのインターフェース
 * @details 武器の発射機構を定義する基底クラス
 */
class IWeaponModule {
public:
	/**
	 * @brief 仮想デストラクタ
	 */
	virtual ~IWeaponModule() = default;

	/**
	 * @brief 武器を発射する
	 * @param entity エンティティ
	 */
	virtual void Execute(Entity& entity) = 0;

	/**
	 * @brief 更新処理
	 * @param deltaTime 前フレームからの経過時間
	 */
	virtual void Update(const float& deltaTime) = 0;

	/**
	 * @brief ImGuiインスペクタ用のUI描画
	 */
	virtual void DrawInspectorImGui() = 0;
};

/**
 * @brief ヒットスキャン武器モジュール
 * @details 即座にヒット判定を行う銃器（ライフルなど）を実装します
 */
class HitscanModule final : public IWeaponModule {
public:
	explicit HitscanModule(const WeaponData& weaponData) :
		mData(weaponData) {
	}

	void Execute(Entity& entity) override;

	void Update(const float& deltaTime) override;

	void DrawInspectorImGui() override;

	[[nodiscard]] const WeaponData& GetWeaponData() const { return mData; }

	[[nodiscard]] Vec3 GetHitPosition() const {
		if (mIsHit) {
			return mHitPosition;
		}
		return Vec3::min; // ヒットしていない場合はあらぬ座標を返す
	}

	Vec3& GetHitNormal() {
		if (mIsHit) {
			return mHitNormal;
		}
		return mHitNormal;
	}

private:
	const WeaponData& mData;

	Vec3 mHitPosition = Vec3::zero; // ヒット位置
	Vec3 mHitNormal   = Vec3::zero; // ヒット面の法線
	bool mIsHit       = false;      // ヒットしたかどうか
};

/**
 * @brief 武器コンポーネント
 * @details エンティティに武器機能を追加するコンポーネント。
 *          発射、リロード、弾薬管理などを行います。
 */
class WeaponComponent : public Component {
public:
	/**
	 * @brief デストラクタ
	 */
	~WeaponComponent() override;

	/**
	 * @brief コンストラクタ
	 * @param weaponJsonPath 武器データのJSONファイルパス
	 */
	explicit WeaponComponent(const std::string& weaponJsonPath);

	/**
	 * @brief エンティティにアタッチされた際に呼ばれる
	 * @param owner 所有者エンティティ
	 */
	void OnAttach(Entity& owner) override;

	/**
	 * @brief 毎フレーム更新処理を行う
	 * @param deltaTime 前フレームからの経過時間
	 */
	void Update(float deltaTime) override;

	/**
	 * @brief ImGuiインスペクタ用のUI描画
	 */
	void DrawInspectorImGui() override;

	/**
	 * @brief トリガーを引く（発射開始）
	 */
	void PullTrigger();

	/**
	 * @brief トリガーを離す（発射停止）
	 */
	void ReleaseTrigger();

	/**
	 * @brief リロードを開始する
	 */
	void Reload();

	/**
	 * @brief 発射可能かどうかを判定する
	 * @return 発射可能な場合true
	 */
	[[nodiscard]] bool CanFire() const;

	/**
	 * @brief ヒット位置を取得する
	 * @return ヒット位置
	 */
	[[nodiscard]] Vec3 GetHitPosition() const;

	/**
	 * @brief ヒット面の法線を取得する
	 * @return ヒット面の法線
	 */
	[[nodiscard]] Vec3& GetHitNormal() const;

	/**
	 * @brief 今フレームで発射したかを判定する
	 * @return 発射した場合true
	 */
	[[nodiscard]] bool HasFiredThisFrame() const;

	/**
	 * @brief 所有者エンティティを取得する
	 * @return 所有者エンティティへのポインタ
	 */
	[[nodiscard]] Entity* GetOwner() const override;

	/**
	 * @brief JSONファイルから武器データを読み込む
	 * @param jsonPath JSONファイルのパス
	 */
	static void LoadFromJson(const std::string& jsonPath);

private:
	struct ActionMapping {
		std::unique_ptr<IWeaponModule> primaryAction;
		std::unique_ptr<IWeaponModule> secondaryAction;
	} mAction;

	std::unique_ptr<WeaponData>    mWeaponData;
	std::unique_ptr<IWeaponModule> mPrimaryModule; // 主モジュール

	Vec3 mHitPosition = Vec3::zero; // ヒット位置

	int   mCurrentAmmo   = 0;
	int   mCurrentClip   = 0;
	float mTimeSinceShot = 0.0f; // 最後に発射してからの時間

	bool  mIsReloading = false; // リロード中かどうか
	float mReloadTimer = 0.0f;  // リロードタイマー

	bool mTriggerHeld = false; // トリガーが押されているかどうか

	bool mFiredThisFrame = false; // 今フレームで発射したかどうか
};
