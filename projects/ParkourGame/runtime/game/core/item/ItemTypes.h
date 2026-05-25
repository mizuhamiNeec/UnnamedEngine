#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Unnamed {
	class JsonReader;
	class JsonWriter;

	/// @brief アイテムの分類です。
	enum class ITEM_CLASS : uint8_t {
		UNKNOWN = 0,
		WEAPON,
		ITEM,
		TOOL,
		QUEST
	};

	/// @brief アイテムのドロップ可否ポリシーです。
	enum class DROP_POLICY : uint8_t {
		ALLOW = 0,
		CANNOT_DROP,
		DROP_ON_DEATH
	};

	/// @brief 武器動作向けの定義データです。
	struct WeaponItemDefinition {
		std::string modeId             = "hitscan";
		float       fireIntervalSec    = 0.2f;
		float       rangeMeters        = 120.0f;
		float       damage             = 10.0f;
		float       radiusMeters       = 0.3f;
		float       projectileSpeedMps = 24.0f;
		bool        automatic          = false;
	};

	/// @brief 静的なアイテム定義データです。
	struct ItemDefinition {
		std::string                         itemId = "item.default";
		std::string                         displayName = "Item";
		ITEM_CLASS                          itemClass = ITEM_CLASS::ITEM;
		bool                                canStack = false;
		uint32_t                            maxStack = 1;
		bool                                autoEquipOnPickup = false;
		DROP_POLICY                         dropPolicy = DROP_POLICY::ALLOW;
		bool                                keepInLoadoutIfEmpty = false;
		std::optional<WeaponItemDefinition> weapon = std::nullopt;
	};

	/// @brief 動的なアイテムインスタンスです。
	struct ItemInstance {
		uint64_t    instanceGuid = 0;
		std::string itemId       = "item.default";
		uint32_t    stackCount   = 1;
	};

	/// @brief ItemClass を文字列へ変換します。
	[[nodiscard]] std::string_view ToString(ITEM_CLASS itemClass);

	/// @brief 文字列から ItemClass を復元します。
	[[nodiscard]] ITEM_CLASS ItemClassFromString(std::string_view text);

	/// @brief DropPolicy を文字列へ変換します。
	[[nodiscard]] std::string_view ToString(DROP_POLICY policy);

	/// @brief 文字列から DropPolicy を復元します。
	[[nodiscard]] DROP_POLICY DropPolicyFromString(std::string_view text);

	/// @brief ItemDefinition を JSON から読み込みます。
	[[nodiscard]] bool DeserializeItemDefinition(
		const JsonReader& reader,
		ItemDefinition&   outDefinition
	);

	/// @brief ItemDefinition を JSON へ書き込みます。
	void SerializeItemDefinition(
		JsonWriter&           writer,
		const ItemDefinition& definition
	);

	/// @brief ItemInstance を JSON から読み込みます。
	[[nodiscard]] bool DeserializeItemInstance(
		const JsonReader& reader,
		ItemInstance&     outInstance
	);

	/// @brief ItemInstance を JSON へ書き込みます。
	void SerializeItemInstance(
		JsonWriter&         writer,
		const ItemInstance& instance
	);
}
