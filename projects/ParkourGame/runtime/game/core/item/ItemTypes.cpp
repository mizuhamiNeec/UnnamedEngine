#include "ItemTypes.h"

#include <algorithm>
#include <cctype>

#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] std::string ToLowerCopy(const std::string_view text) {
			std::string out(text);
			std::ranges::transform(
				out,
				out.begin(),
				[](const unsigned char c) {
					return static_cast<char>(std::tolower(c));
				}
			);
			return out;
		}
	}

	std::string_view ToString(const ITEM_CLASS itemClass) {
		switch (itemClass) {
			case ITEM_CLASS::WEAPON: return "weapon";
			case ITEM_CLASS::ITEM: return "item";
			case ITEM_CLASS::TOOL: return "tool";
			case ITEM_CLASS::QUEST: return "quest";
			default: return "unknown";
		}
	}

	ITEM_CLASS ItemClassFromString(const std::string_view text) {
		const std::string lowered = ToLowerCopy(text);
		if (lowered == "weapon") {
			return ITEM_CLASS::WEAPON;
		}
		if (lowered == "item") {
			return ITEM_CLASS::ITEM;
		}
		if (lowered == "tool") {
			return ITEM_CLASS::TOOL;
		}
		if (lowered == "quest") {
			return ITEM_CLASS::QUEST;
		}
		return ITEM_CLASS::UNKNOWN;
	}

	std::string_view ToString(const DROP_POLICY policy) {
		switch (policy) {
			case DROP_POLICY::ALLOW: return "allow";
			case DROP_POLICY::CANNOT_DROP: return "cannot_drop";
			case DROP_POLICY::DROP_ON_DEATH: return "drop_on_death";
			default: return "allow";
		}
	}

	DROP_POLICY DropPolicyFromString(const std::string_view text) {
		const std::string lowered = ToLowerCopy(text);
		if (lowered == "cannot_drop") {
			return DROP_POLICY::CANNOT_DROP;
		}
		if (lowered == "drop_on_death") {
			return DROP_POLICY::DROP_ON_DEATH;
		}
		return DROP_POLICY::ALLOW;
	}

	bool DeserializeItemDefinition(
		const JsonReader& reader,
		ItemDefinition&   outDefinition
	) {
		if (!reader.Valid() || !reader.IsObject()) {
			return false;
		}

		ItemDefinition definition = outDefinition;
		definition.itemId = reader["itemId"].GetString(definition.itemId);
		if (definition.itemId.empty()) {
			return false;
		}
		definition.displayName = reader["displayName"].GetString(
			definition.displayName
		);
		definition.itemClass = ItemClassFromString(
			reader["itemClass"].GetString(
				std::string(
					ToString(
						definition.itemClass
					)
				)
			)
		);
		definition.canStack = reader["canStack"].GetBool(definition.canStack);
		definition.maxStack = static_cast<uint32_t>(std::max(
			1,
			reader["maxStack"].GetInt(static_cast<int>(definition.maxStack))
		));
		definition.autoEquipOnPickup = reader["autoEquipOnPickup"].GetBool(
			definition.autoEquipOnPickup
		);
		definition.dropPolicy = DropPolicyFromString(
			reader["dropPolicy"].GetString(
				std::string(
					ToString(
						definition.dropPolicy
					)
				)
			)
		);
		definition.keepInLoadoutIfEmpty = reader["keepInLoadoutIfEmpty"].
			GetBool(
				definition.keepInLoadoutIfEmpty
			);

		if (const JsonReader weapon = reader["weapon"];
			weapon.Valid() && weapon.IsObject()) {
			WeaponItemDefinition weaponDefinition = definition.weapon.value_or(
				WeaponItemDefinition{}
			);
			weaponDefinition.modeId = weapon["modeId"].GetString(
				weaponDefinition.modeId
			);
			weaponDefinition.fireIntervalSec = std::max(
				0.0f,
				weapon["fireIntervalSec"].GetFloat(
					weaponDefinition.fireIntervalSec
				)
			);
			weaponDefinition.rangeMeters = std::max(
				0.01f,
				weapon["rangeMeters"].GetFloat(weaponDefinition.rangeMeters)
			);
			weaponDefinition.damage = weapon["damage"].GetFloat(
				weaponDefinition.damage
			);
			weaponDefinition.radiusMeters = std::max(
				0.01f,
				weapon["radiusMeters"].GetFloat(weaponDefinition.radiusMeters)
			);
			weaponDefinition.projectileSpeedMps = std::max(
				0.0f,
				weapon["projectileSpeedMps"].GetFloat(
					weaponDefinition.projectileSpeedMps
				)
			);
			weaponDefinition.automatic = weapon["automatic"].GetBool(
				weaponDefinition.automatic
			);
			definition.weapon = weaponDefinition;
		} else {
			definition.weapon = std::nullopt;
		}

		outDefinition = std::move(definition);
		return true;
	}

	void SerializeItemDefinition(
		JsonWriter&           writer,
		const ItemDefinition& definition
	) {
		writer.Key("itemId");
		writer.Write(definition.itemId);
		writer.Key("displayName");
		writer.Write(definition.displayName);
		writer.Key("itemClass");
		writer.Write(std::string(ToString(definition.itemClass)));
		writer.Key("canStack");
		writer.Write(definition.canStack);
		writer.Key("maxStack");
		writer.Write(static_cast<int>(definition.maxStack));
		writer.Key("autoEquipOnPickup");
		writer.Write(definition.autoEquipOnPickup);
		writer.Key("dropPolicy");
		writer.Write(std::string(ToString(definition.dropPolicy)));
		writer.Key("keepInLoadoutIfEmpty");
		writer.Write(definition.keepInLoadoutIfEmpty);
		if (definition.weapon.has_value()) {
			writer.Key("weapon");
			writer.BeginObject();
			writer.Key("modeId");
			writer.Write(definition.weapon->modeId);
			writer.Key("fireIntervalSec");
			writer.Write(definition.weapon->fireIntervalSec);
			writer.Key("rangeMeters");
			writer.Write(definition.weapon->rangeMeters);
			writer.Key("damage");
			writer.Write(definition.weapon->damage);
			writer.Key("radiusMeters");
			writer.Write(definition.weapon->radiusMeters);
			writer.Key("projectileSpeedMps");
			writer.Write(definition.weapon->projectileSpeedMps);
			writer.Key("automatic");
			writer.Write(definition.weapon->automatic);
			writer.EndObject();
		}
	}

	bool DeserializeItemInstance(
		const JsonReader& reader,
		ItemInstance&     outInstance
	) {
		if (!reader.Valid() || !reader.IsObject()) {
			return false;
		}

		ItemInstance instance = outInstance;
		instance.instanceGuid = reader["instanceGuid"].GetUint64();
		instance.itemId       = reader["itemId"].GetString(instance.itemId);
		if (instance.itemId.empty()) {
			return false;
		}
		instance.stackCount = static_cast<uint32_t>(std::max(
			1,
			reader["stackCount"].GetInt(static_cast<int>(instance.stackCount))
		));

		outInstance = std::move(instance);
		return true;
	}

	void SerializeItemInstance(
		JsonWriter&         writer,
		const ItemInstance& instance
	) {
		writer.Key("instanceGuid");
		writer.Write(instance.instanceGuid);
		writer.Key("itemId");
		writer.Write(instance.itemId);
		writer.Key("stackCount");
		writer.Write(static_cast<int>(instance.stackCount));
	}
}
