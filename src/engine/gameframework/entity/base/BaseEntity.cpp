#include <engine/gameframework/entity/base/BaseEntity.h>

namespace Unnamed {
	/// @brief コンストラクタ
	/// @param name エンティティの名前
	BaseEntity::BaseEntity(std::string name) : mName(std::move(name)) {
	}

	/// @brief デストラクタ
	BaseEntity::~BaseEntity() = default;

	/// @brief エディターのティック時に呼び出されます。
	/// @param deltaTime 前のフレームからの経過時間（秒）
	void BaseEntity::OnEditorTick(const float deltaTime) {
		(void)deltaTime;
	}

	/// @brief エディターのレンダリング時に呼び出されます。
	void BaseEntity::OnEditorRender() const {
	}

	/// @brief エンティティの名前を取得します。
	/// @return エンティティの名前
	std::string_view BaseEntity::GetName() const {
		return mName;
	}

	/// @brief エンティティの名前を設定します。
	/// @param name エンティティの名前
	void BaseEntity::SetName(std::string& name) {
		mName = std::move(name);
	}

	/// @brief エディター専用エンティティかどうかを取得します。
	/// @return エディター専用エンティティか?
	bool BaseEntity::IsEditorOnly() const noexcept {
		return mIsEditorOnly;
	}

	/// @brief エンティティのIDを取得します。
	/// @return エンティティのID
	uint64_t BaseEntity::GetId() const noexcept {
		return mId;
	}
}
