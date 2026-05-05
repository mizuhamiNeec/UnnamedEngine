#pragma once

#include <string_view>

#include "engine/unnamed/framework/components/base/BaseComponent.h"

#include "game/core/item/ItemTypes.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;

	/// @brief フィールド上に存在する拾得可能アイテムを表すコンポーネントです。
	class WorldItemComponent final : public BaseComponent {
	public:
		/// @brief アイテムインスタンスを設定します。
		void SetItemInstance(const ItemInstance& instance);

		/// @brief 設定済みのアイテムインスタンスを参照します。
		[[nodiscard]] const ItemInstance& GetItemInstance() const;

		/// @brief まだ拾得可能かを返します。
		[[nodiscard]] bool IsAvailable() const;

		/// @brief アイテムを取得し、未取得状態から取得済みへ遷移します。
		/// @return 取得に成功した場合 true
		bool TryTakeItemInstance(ItemInstance& outInstance);

		/// @brief 拾得後にエンティティを破棄するかを設定します。
		void SetDestroyOwnerOnTake(bool enabled);

		/// @brief 拾得後にエンティティを破棄するかを返します。
		[[nodiscard]] bool GetDestroyOwnerOnTake() const;

		/// @brief シリアライズ時に使用する安定名を返します。
		[[nodiscard]] std::string_view GetStableName() const override;

		/// @brief 表示名を返します。
		[[nodiscard]] std::string_view GetComponentName() const override;

		/// @brief エディタアイコンを返します。
		[[nodiscard]] uint32_t GetIcon() const override;

		/// @brief JSONから設定を読み込みます。
		void Deserialize(const JsonReader& reader) override;

		/// @brief JSONへ設定を書き込みます。
		void Serialize(JsonWriter& writer) const override;

	private:
		/// @brief ワールド上で保持しているアイテムインスタンスです。
		ItemInstance mItemInstance = {};
		/// @brief 未取得であれば true です。
		bool mAvailable = true;
		/// @brief 取得成功時に所有エンティティを破棄するかです。
		bool mDestroyOwnerOnTake = true;
	};
}
