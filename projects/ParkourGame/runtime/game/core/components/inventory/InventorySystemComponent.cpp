#include "InventorySystemComponent.h"

#include <algorithm>
#include <imgui.h>
#include <utility>

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/Icons.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/world/World.h"

#include "engine/scene/Scene.h"

#include "game/core/components/inventory/WorldItemComponent.h"
#include "game/core/item/ItemEventIds.h"

namespace Unnamed {
	namespace {
		static constexpr std::string_view kChannel        = "InventorySystem";
		static constexpr std::string_view kFallbackItemId = "weapon.fists";

		[[nodiscard]] float ToFloat(const size_t value) {
			return static_cast<float>(value);
		}
	}

	void InventorySystemComponent::EnqueueDeterministicActionInput(
		const uint64_t                   tick,
		const float                      stepSeconds,
		const CharacterActionFrameInput& input
	) {
		mDeterministicActionInputQueue.Push(
			DeterministicActionInputPacket{
				.tick        = tick,
				.stepSeconds = stepSeconds,
				.input       = input
			}
		);
	}

	void InventorySystemComponent::ClearDeterministicActionInputQueue() {
		mDeterministicActionInputQueue.Clear();
	}

	bool InventorySystemComponent::RegisterDefinition(
		const ItemDefinition& definition
	) {
		if (definition.itemId.empty()) {
			return false;
		}
		mDefinitions[definition.itemId] = definition;
		return true;
	}

	const ItemDefinition* InventorySystemComponent::FindDefinition(
		const std::string_view itemId
	) const {
		if (itemId.empty()) {
			return nullptr;
		}
		const auto it = mDefinitions.find(std::string(itemId));
		return it == mDefinitions.end() ? nullptr : &it->second;
	}

	const ItemInstance* InventorySystemComponent::GetEquippedItem() const {
		if (
			mEquippedIndex < 0 ||
			std::cmp_greater_equal(mEquippedIndex, mEntries.size())
		) {
			return nullptr;
		}
		return &mEntries[static_cast<size_t>(mEquippedIndex)].instance;
	}

	const ItemDefinition*
	InventorySystemComponent::GetEquippedDefinition() const {
		const ItemInstance* equipped = GetEquippedItem();
		return equipped ? FindDefinition(equipped->itemId) : nullptr;
	}

	bool InventorySystemComponent::TryGetEquippedWeaponDefinition(
		WeaponItemDefinition& outWeaponDefinition
	) const {
		const ItemDefinition* definition = GetEquippedDefinition();
		if (!definition || !definition->weapon.has_value()) {
			return false;
		}
		outWeaponDefinition = definition->weapon.value();
		return true;
	}

	size_t InventorySystemComponent::GetItemCount() const {
		return mEntries.size();
	}

	int32_t InventorySystemComponent::GetEquippedIndex() const {
		return mEquippedIndex;
	}

	bool InventorySystemComponent::TryPickupItem(const ItemInstance& instance) {
		return TryPickupItemInternal(instance, true);
	}

	bool InventorySystemComponent::TryPickupFromWorldEntity(
		const uint64_t worldItemEntityGuid
	) {
		// GUID からワールドアイテム実体を引き当てます。
		WorldItemComponent* worldItem = ResolveWorldItemComponent(
			worldItemEntityGuid
		);
		if (!worldItem) {
			PublishInventoryCue(ItemEventIds::kPickupRejected);
			return false;
		}

		const ItemInstance candidate = worldItem->GetItemInstance();
		if (!CanAcceptItem(candidate)) {
			PublishInventoryCue(ItemEventIds::kPickupRejected);
			return false;
		}

		// 競合を避けるため、先にワールド側から所有権を取り出します。
		ItemInstance instance = {};
		if (!worldItem->TryTakeItemInstance(instance)) {
			PublishInventoryCue(ItemEventIds::kPickupRejected);
			return false;
		}

		const bool picked = TryPickupItem(instance);
		if (!picked) {
			return false;
		}

		// 取得済みアイテムは必要に応じてワールドエンティティを破棄します。
		if (worldItem->GetDestroyOwnerOnTake()) {
			if (Entity* itemOwner = worldItem->GetOwner()) {
				itemOwner->MarkPendingDestroy();
			}
		}
		return true;
	}

	bool InventorySystemComponent::TryDropEquippedItem() {
		if (mEquippedIndex < 0) {
			PublishInventoryCue(ItemEventIds::kDropRejected);
			return false;
		}
		return TryDropItemByIndex(static_cast<size_t>(mEquippedIndex));
	}

	bool InventorySystemComponent::TryDropItemByIndex(const size_t index) {
		if (!IsValidIndex(index)) {
			PublishInventoryCue(ItemEventIds::kDropRejected);
			return false;
		}

		const InventoryEntry* entry = GetEntry(index);
		if (!entry || !CanDropEntry(*entry)) {
			PublishInventoryCue(ItemEventIds::kDropRejected);
			return false;
		}
		PublishInventoryCue(ItemEventIds::kDropRequested, ToFloat(index));

		if (std::cmp_equal(mEquippedIndex, index)) {
			(void)TryUnequipCurrent();
		}

		// ドロップ用のエンティティをスポーンして WorldItem を付与します。
		ItemInstance dropped = entry->instance;
		if (Scene* scene = GetScene()) {
			Entity& droppedEntity = scene->CreateEntity(
				"DroppedItem",
				0,
				false
			);
			if (Entity* owner = GetOwner()) {
				droppedEntity.SetFolderPath(owner->GetFolderPath());
			}

			auto* droppedTransform = droppedEntity.GetOrAddComponent<
				TransformComponent>();
			if (Entity* owner = GetOwner()) {
				if (auto* ownerTransform = owner->GetComponent<
						TransformComponent>()
				) {
					droppedTransform->SetPosition(
						ownerTransform->GetPosition() +
						ownerTransform->Forward() * 0.6f +
						Vec3(0.0f, 0.6f, 0.0f)
					);
				}
			}
			auto* worldItem = droppedEntity.AddComponent<WorldItemComponent>();
			worldItem->SetItemInstance(dropped);
			worldItem->SetDestroyOwnerOnTake(true);
		}

		RemoveEntryAt(index);
		EnsureFallbackEntry();
		if (mEquippedIndex < 0 && !mEntries.empty()) {
			(void)EquipIndexInternal(0, true);
		}

		PublishInventoryCue(
			ItemEventIds::kDropSucceeded,
			ToFloat(index),
			static_cast<float>(dropped.stackCount)
		);
		return true;
	}

	bool InventorySystemComponent::TryEquipByIndex(const size_t index) {
		PublishInventoryCue(ItemEventIds::kEquipRequested, ToFloat(index));
		return EquipIndexInternal(index, true);
	}

	bool InventorySystemComponent::TryUnequipCurrent() {
		PublishInventoryCue(
			ItemEventIds::kUnequipRequested,
			static_cast<float>(mEquippedIndex)
		);
		if (
			mEquippedIndex < 0 ||
			std::cmp_greater_equal(mEquippedIndex, mEntries.size())
		) {
			PublishInventoryCue(ItemEventIds::kUnequipRejected);
			return false;
		}
		mEquippedIndex = -1;
		PublishInventoryCue(ItemEventIds::kUnequipSucceeded);
		return true;
	}

	void InventorySystemComponent::OnAttached() {
		BaseComponent::OnAttached();
		ClearDeterministicActionInputQueue();
		mActionFrameInput = {};
		EnsureCoreDefinitions();
		EnsureFallbackEntry();
	}

	void InventorySystemComponent::OnDetached() {
		ClearDeterministicActionInputQueue();
		mActionFrameInput = {};
		BaseComponent::OnDetached();
	}

	void InventorySystemComponent::OnTick(const float deltaTime) {
		(void)deltaTime;
		// deterministic キューから 1 件だけ消費してこのティックの入力とします。
		DeterministicActionInputPacket packet = {};
		if (!mDeterministicActionInputQueue.Pop(packet)) {
			mActionFrameInput = {};
			return;
		}
		mActionFrameInput = packet.input;
		ApplyCycleInput(mActionFrameInput.weapon);
	}

	std::string_view InventorySystemComponent::GetStableName() const {
		return "game.InventorySystem";
	}

	std::string_view InventorySystemComponent::GetComponentName() const {
		return "InventorySystem";
	}

	uint32_t InventorySystemComponent::GetIcon() const {
		return kIconFolder;
	}

#ifdef _DEBUG
	void InventorySystemComponent::DrawInspectorImGui() {
		ImGui::DragInt(
			"Equipped Index",
			&mEquippedIndex,
			1.0f,
			-1,
			static_cast<int>(mEntries.size()) - 1
		);
		int maxEntries = static_cast<int>(mMaxEntries);
		if (ImGui::DragInt("Max Entries", &maxEntries, 1.0f, 1, 128)) {
			mMaxEntries = static_cast<uint32_t>(std::max(1, maxEntries));
		}
		ImGui::Checkbox("Cycle Wrap", &mAllowCycleWrap);
		ImGui::Checkbox("Auto Equip On Pickup", &mAutoEquipOnPickup);
		ImGui::Separator();
		ImGui::Text("Entries: %d", static_cast<int>(mEntries.size()));
		for (size_t i = 0; i < mEntries.size(); ++i) {
			const InventoryEntry& entry = mEntries[i];
			const ItemDefinition* def   = FindDefinition(entry.instance.itemId);
			ImGui::PushID(static_cast<int>(i));
			ImGui::Text(
				"[%d] %s x%u (%s)%s",
				static_cast<int>(i),
				entry.instance.itemId.c_str(),
				entry.instance.stackCount,
				def ?
					std::string(ToString(def->dropPolicy)).c_str() :
					"unknown",
				(std::cmp_equal(mEquippedIndex, i)) ? " <equipped>" : ""
			);
			if (ImGui::Button("Equip")) {
				(void)TryEquipByIndex(i);
			}
			ImGui::SameLine();
			if (ImGui::Button("Drop")) {
				(void)TryDropItemByIndex(i);
			}
			ImGui::PopID();
		}
	}
#endif

	void InventorySystemComponent::Deserialize(const JsonReader& reader) {
		mDefinitions.clear();
		mEntries.clear();
		mEquippedIndex    = reader["equippedIndex"].GetInt(mEquippedIndex);
		mNextInstanceGuid = std::max<uint64_t>(
			1ull,
			reader["nextInstanceGuid"].GetUint64()
		);
		mMaxEntries = static_cast<uint32_t>(std::max(
			1,
			reader["maxEntries"].GetInt(static_cast<int>(mMaxEntries))
		));
		mAllowCycleWrap    = reader["allowCycleWrap"].GetBool(mAllowCycleWrap);
		mAutoEquipOnPickup = reader["autoEquipOnPickup"].GetBool(
			mAutoEquipOnPickup
		);

		if (const JsonReader definitions = reader["definitions"];
			definitions.Valid() && definitions.IsArray()) {
			// 定義配列は不正要素をスキップしつつ復元します。
			for (size_t i = 0; i < definitions.Size(); ++i) {
				ItemDefinition definition = {};
				if (DeserializeItemDefinition(definitions[i], definition)) {
					(void)RegisterDefinition(definition);
				}
			}
		}

		if (const JsonReader entries = reader["entries"];
			entries.Valid() && entries.IsArray()) {
			// エントリ配列は未知定義や壊れた値を除外して取り込みます。
			for (size_t i = 0; i < entries.Size(); ++i) {
				ItemInstance instance = {};
				if (!DeserializeItemInstance(entries[i], instance)) {
					continue;
				}
				if (!FindDefinition(instance.itemId)) {
					Warning(
						kChannel,
						"Skipped unknown item definition '{}'.",
						instance.itemId
					);
					continue;
				}
				if (instance.instanceGuid == 0) {
					instance.instanceGuid = AllocateInstanceGuid();
				}
				mEntries.emplace_back(
					InventoryEntry{
						.instance = std::move(instance)
					}
				);
			}
		}

		EnsureCoreDefinitions();
		EnsureFallbackEntry();
		if (!mEntries.empty()) {
			mEquippedIndex = std::clamp(
				mEquippedIndex,
				0,
				static_cast<int32_t>(mEntries.size()) - 1
			);
		} else {
			mEquippedIndex = -1;
		}
	}

	void InventorySystemComponent::Serialize(JsonWriter& writer) const {
		writer.Key("equippedIndex");
		writer.Write(mEquippedIndex);
		writer.Key("nextInstanceGuid");
		writer.Write(mNextInstanceGuid);
		writer.Key("maxEntries");
		writer.Write(static_cast<int>(mMaxEntries));
		writer.Key("allowCycleWrap");
		writer.Write(mAllowCycleWrap);
		writer.Key("autoEquipOnPickup");
		writer.Write(mAutoEquipOnPickup);

		writer.Key("definitions");
		writer.BeginArray();
		for (const auto& [id, definition] : mDefinitions) {
			(void)id;
			writer.BeginObject();
			SerializeItemDefinition(writer, definition);
			writer.EndObject();
		}
		writer.EndArray();

		writer.Key("entries");
		writer.BeginArray();
		for (const InventoryEntry& entry : mEntries) {
			writer.BeginObject();
			SerializeItemInstance(writer, entry.instance);
			writer.EndObject();
		}
		writer.EndArray();
	}

	void InventorySystemComponent::EnsureCoreDefinitions() {
		if (FindDefinition(kFallbackItemId) != nullptr) {
			return;
		}
		ItemDefinition fists       = {};
		fists.itemId               = std::string(kFallbackItemId);
		fists.displayName          = "Fists";
		fists.itemClass            = ITEM_CLASS::WEAPON;
		fists.canStack             = false;
		fists.maxStack             = 1;
		fists.autoEquipOnPickup    = true;
		fists.dropPolicy           = DROP_POLICY::CANNOT_DROP;
		fists.keepInLoadoutIfEmpty = true;
		fists.weapon               = WeaponItemDefinition{
			.modeId             = "melee",
			.fireIntervalSec    = 0.35f,
			.rangeMeters        = 1.8f,
			.damage             = 22.0f,
			.radiusMeters       = 0.45f,
			.projectileSpeedMps = 0.0f,
			.automatic          = false
		};
		(void)RegisterDefinition(fists);
	}

	void InventorySystemComponent::EnsureFallbackEntry() {
		EnsureCoreDefinitions();
		if (mEntries.empty()) {
			// 武器ゼロ状態を防ぐため、最低 1 件はフォールバックを保持します。
			ItemInstance fallback = {};
			fallback.instanceGuid = AllocateInstanceGuid();
			fallback.itemId       = std::string(kFallbackItemId);
			fallback.stackCount   = 1;
			mEntries.emplace_back(
				InventoryEntry{.instance = std::move(fallback)}
			);
		}

		if (mEquippedIndex < 0 ||
		    std::cmp_greater_equal(mEquippedIndex, mEntries.size())) {
			mEquippedIndex = 0;
		}
	}

	void InventorySystemComponent::ApplyCycleInput(
		const WeaponActionInput& input
	) {
		if (input.cycleNextPressed && !input.cyclePrevPressed) {
			(void)SelectRelativeEntry(1);
		} else if (!input.cycleNextPressed && input.cyclePrevPressed) {
			(void)SelectRelativeEntry(-1);
		}
	}

	bool InventorySystemComponent::SelectRelativeEntry(const int32_t delta) {
		if (mEntries.empty()) {
			return false;
		}

		const int32_t count  = static_cast<int32_t>(mEntries.size());
		int32_t       target = mEquippedIndex >= 0 ? mEquippedIndex : 0;
		target               += delta;

		// ラップ有効時は循環、無効時は端でクランプします。
		if (mAllowCycleWrap) {
			target %= count;
			if (target < 0) {
				target += count;
			}
		} else {
			target = std::clamp(target, 0, count - 1);
		}
		return TryEquipByIndex(static_cast<size_t>(target));
	}

	bool InventorySystemComponent::CanAcceptItem(
		const ItemInstance& instance
	) const {
		if (instance.itemId.empty()) {
			return false;
		}
		const ItemDefinition* definition = FindDefinition(instance.itemId);
		if (!definition) {
			return false;
		}

		if (!definition->canStack) {
			return mEntries.size() < mMaxEntries;
		}

		const uint32_t safeMaxStack = std::max<uint32_t>(
			1u, definition->maxStack
		);
		uint32_t remaining = std::max<uint32_t>(1u, instance.stackCount);
		size_t   freeSlots = mEntries.size() < mMaxEntries ?
			                     (mMaxEntries - mEntries.size()) :
			                     0u;

		// 既存スタックへ積める分を先に見積もります。
		for (const InventoryEntry& entry : mEntries) {
			if (entry.instance.itemId != instance.itemId) {
				continue;
			}
			if (entry.instance.stackCount >= safeMaxStack) {
				continue;
			}
			const uint32_t space = safeMaxStack - entry.instance.stackCount;
			if (space >= remaining) {
				return true;
			}
			remaining -= space;
		}

		// 残量を新規スロットで吸収できるかをシミュレートします。
		while (remaining > 0u) {
			if (freeSlots == 0u) {
				return false;
			}
			const uint32_t consumed = std::min<uint32_t>(
				remaining, safeMaxStack
			);
			remaining -= consumed;
			--freeSlots;
		}
		return true;
	}

	bool InventorySystemComponent::TryPickupItemInternal(
		const ItemInstance& instance,
		const bool          emitEvents
	) {
		if (emitEvents) {
			PublishInventoryCue(
				ItemEventIds::kPickupRequested,
				static_cast<float>(instance.stackCount)
			);
		}
		if (!CanAcceptItem(instance)) {
			if (emitEvents) {
				PublishInventoryCue(ItemEventIds::kPickupRejected);
			}
			return false;
		}

		const ItemDefinition* definition = FindDefinition(instance.itemId);
		if (!definition) {
			if (emitEvents) {
				PublishInventoryCue(ItemEventIds::kPickupRejected);
			}
			return false;
		}

		const uint32_t safeMaxStack = std::max<uint32_t>(
			1u, definition->maxStack
		);
		uint32_t remaining      = std::max<uint32_t>(1u, instance.stackCount);
		int32_t  equipCandidate = -1;

		if (definition->canStack) {
			// 既存スタックを優先して加算し、空きスロット消費を抑えます。
			for (size_t i = 0; i < mEntries.size() && remaining > 0u; ++i) {
				InventoryEntry& entry = mEntries[i];
				if (entry.instance.itemId != instance.itemId) {
					continue;
				}
				if (entry.instance.stackCount >= safeMaxStack) {
					continue;
				}
				const uint32_t space = safeMaxStack - entry.instance.stackCount;
				const uint32_t add = std::min<uint32_t>(space, remaining);
				entry.instance.stackCount += add;
				remaining -= add;
				if (equipCandidate < 0) {
					equipCandidate = static_cast<int32_t>(i);
				}
			}
		}

		// 残りは新規エントリを作って格納します。
		while (remaining > 0u) {
			if (mEntries.size() >= mMaxEntries) {
				if (emitEvents) {
					PublishInventoryCue(ItemEventIds::kPickupRejected);
				}
				return false;
			}
			ItemInstance newInstance = instance;
			newInstance.instanceGuid = AllocateInstanceGuid();
			newInstance.stackCount   = definition->canStack ?
				                           std::min<uint32_t>(
					                           remaining, safeMaxStack
				                           ) :
				                           1u;
			remaining -= newInstance.stackCount;
			mEntries.emplace_back(
				InventoryEntry{
					.instance = std::move(newInstance)
				}
			);
			if (equipCandidate < 0) {
				equipCandidate = static_cast<int32_t>(mEntries.size()) - 1;
			}
		}

		if (
			equipCandidate >= 0 &&
			(definition->autoEquipOnPickup || mAutoEquipOnPickup)
		) {
			// 定義かシステム設定で auto equip が有効なら即装備します。
			(void)EquipIndexInternal(
				static_cast<size_t>(equipCandidate), emitEvents
			);
		}

		if (emitEvents) {
			PublishInventoryCue(
				ItemEventIds::kPickupSucceeded,
				static_cast<float>(instance.stackCount),
				static_cast<float>(mEntries.size())
			);
		}
		return true;
	}

	bool InventorySystemComponent::EquipIndexInternal(
		const size_t index,
		const bool   emitEvents
	) {
		if (!IsValidIndex(index)) {
			if (emitEvents) {
				PublishInventoryCue(
					ItemEventIds::kEquipRejected, ToFloat(index)
				);
			}
			return false;
		}

		const int32_t target = static_cast<int32_t>(index);
		if (mEquippedIndex == target) {
			// 同一装備要求は成功扱いにして上位ロジックを単純化します。
			if (emitEvents) {
				PublishInventoryCue(
					ItemEventIds::kEquipSucceeded, ToFloat(index)
				);
			}
			return true;
		}

		if (
			mEquippedIndex >= 0 &&
			std::cmp_less(mEquippedIndex, mEntries.size()) &&
			emitEvents
		) {
			// 既存装備がある場合は明示的に unequip シーケンスを発火します。
			PublishInventoryCue(
				ItemEventIds::kUnequipRequested,
				static_cast<float>(mEquippedIndex)
			);
		}
		if (
			mEquippedIndex >= 0 &&
			std::cmp_less(mEquippedIndex, mEntries.size())
		) {
			mEquippedIndex = -1;
			if (emitEvents) {
				PublishInventoryCue(
					ItemEventIds::kUnequipSucceeded,
					static_cast<float>(index)
				);
			}
		}

		mEquippedIndex = target;
		if (emitEvents) {
			PublishInventoryCue(ItemEventIds::kEquipSucceeded, ToFloat(index));
		}
		return true;
	}

	bool InventorySystemComponent::IsValidIndex(const size_t index) const {
		return index < mEntries.size();
	}

	const InventorySystemComponent::InventoryEntry*
	InventorySystemComponent::GetEntry(const size_t index) const {
		return IsValidIndex(index) ? &mEntries[index] : nullptr;
	}

	bool InventorySystemComponent::CanDropEntry(
		const InventoryEntry& entry
	) const {
		const ItemDefinition* definition = FindDefinition(
			entry.instance.itemId
		);
		if (!definition) {
			return false;
		}
		if (definition->dropPolicy == DROP_POLICY::CANNOT_DROP) {
			return false;
		}
		return true;
	}

	void InventorySystemComponent::RemoveEntryAt(const size_t index) {
		if (!IsValidIndex(index)) {
			return;
		}

		if (std::cmp_equal(mEquippedIndex, index)) {
			mEquippedIndex = -1;
		} else if (std::cmp_greater(mEquippedIndex, index)) {
			--mEquippedIndex;
		}
		mEntries.erase(mEntries.begin() + static_cast<int64_t>(index));
	}

	uint64_t InventorySystemComponent::AllocateInstanceGuid() {
		return mNextInstanceGuid++;
	}

	WorldItemComponent* InventorySystemComponent::ResolveWorldItemComponent(
		const uint64_t worldItemEntityGuid
	) const {
		if (worldItemEntityGuid == 0) {
			return nullptr;
		}
		// GUID -> Entity -> WorldItemComponent の順で安全に解決します。
		const Scene* scene = GetScene();
		if (!scene) {
			return nullptr;
		}
		Entity* entity = const_cast<Scene*>(scene)->FindEntity(
			worldItemEntityGuid
		);
		if (!entity) {
			return nullptr;
		}
		return entity->GetComponent<WorldItemComponent>();
	}

	void InventorySystemComponent::PublishInventoryCue(
		const std::string_view id,
		const float            value,
		const float            value2
	) const {
		// sourceEntityGuid を必ず付与して、後段で発火源を追跡できるようにします。
		World*  world = GetWorld();
		Entity* owner = GetOwner();
		if (!world || !owner) {
			return;
		}
		GameplayCue cue      = {};
		cue.id               = std::string(id);
		cue.sourceEntityGuid = owner->GetGuid();
		cue.value            = value;
		cue.value2           = value2;
		world->GetGameplayCueBus().Publish(cue);
	}

	REGISTER_COMPONENT(InventorySystemComponent);
}
