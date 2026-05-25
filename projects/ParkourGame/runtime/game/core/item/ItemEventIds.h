#pragma once

#include <string_view>

namespace Unnamed::ItemEventIds {
	/// @brief アイテム拾得要求イベントです。
	inline constexpr std::string_view kPickupRequested =
		"inventory.pickup.requested";

	/// @brief アイテム拾得成功イベントです。
	inline constexpr std::string_view kPickupSucceeded =
		"inventory.pickup.succeeded";

	/// @brief アイテム拾得失敗イベントです。
	inline constexpr std::string_view kPickupRejected =
		"inventory.pickup.rejected";

	/// @brief アイテムドロップ要求イベントです。
	inline constexpr std::string_view kDropRequested =
		"inventory.drop.requested";

	/// @brief アイテムドロップ成功イベントです。
	inline constexpr std::string_view kDropSucceeded =
		"inventory.drop.succeeded";

	/// @brief アイテムドロップ失敗イベントです。
	inline constexpr std::string_view kDropRejected = "inventory.drop.rejected";

	/// @brief アイテム装備要求イベントです。
	inline constexpr std::string_view kEquipRequested =
		"inventory.equip.requested";

	/// @brief アイテム装備成功イベントです。
	inline constexpr std::string_view kEquipSucceeded =
		"inventory.equip.succeeded";

	/// @brief アイテム装備失敗イベントです。
	inline constexpr std::string_view kEquipRejected =
		"inventory.equip.rejected";

	/// @brief アイテム装備解除要求イベントです。
	inline constexpr std::string_view kUnequipRequested =
		"inventory.unequip.requested";

	/// @brief アイテム装備解除成功イベントです。
	inline constexpr std::string_view kUnequipSucceeded =
		"inventory.unequip.succeeded";

	/// @brief アイテム装備解除失敗イベントです。
	inline constexpr std::string_view kUnequipRejected =
		"inventory.unequip.rejected";
}
