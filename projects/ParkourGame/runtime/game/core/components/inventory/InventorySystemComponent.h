#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/containers/RingBuffer.h"

#include "engine/unnamed/framework/components/base/BaseComponent.h"

#include "game/core/input/CharacterActionFrameInput.h"
#include "game/core/item/ItemTypes.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;
	class WorldItemComponent;

	/// @brief アイテムの所持/装備/拾得/ドロップを管理するコンポーネントです。
	class InventorySystemComponent final :
		public BaseComponent, public ICharacterActionInputReceiver {
	public:
		/// @brief deterministic アクション入力キューの最大保持件数です。
		static constexpr size_t kDeterministicActionInputQueueCapacity = 128;

		/// @brief deterministic アクション入力をキューへ追加します。
		void EnqueueDeterministicActionInput(
			uint64_t                         tick,
			float                            stepSeconds,
			const CharacterActionFrameInput& input
		) override;

		/// @brief deterministic アクション入力キューをクリアします。
		void ClearDeterministicActionInputQueue() override;

		/// @brief アイテム定義を登録します。
		/// @return 登録できた場合 true
		bool RegisterDefinition(const ItemDefinition& definition);

		/// @brief 定義IDでアイテム定義を検索します。
		[[nodiscard]] const ItemDefinition* FindDefinition(
			std::string_view itemId
		) const;

		/// @brief 現在装備中のアイテムインスタンスを返します。
		[[nodiscard]] const ItemInstance* GetEquippedItem() const;

		/// @brief 現在装備中のアイテム定義を返します。
		[[nodiscard]] const ItemDefinition* GetEquippedDefinition() const;

		/// @brief 装備中アイテムが武器定義を持つ場合に取得します。
		[[nodiscard]] bool TryGetEquippedWeaponDefinition(
			WeaponItemDefinition& outWeaponDefinition
		) const;

		/// @brief インベントリ内アイテム数を返します。
		[[nodiscard]] size_t GetItemCount() const;

		/// @brief 装備中インデックスを返します。
		[[nodiscard]] int32_t GetEquippedIndex() const;

		/// @brief アイテムを拾得します。
		/// @return 成功した場合 true
		bool TryPickupItem(const ItemInstance& instance);

		/// @brief ワールド上のアイテムエンティティから拾得します。
		/// @return 成功した場合 true
		bool TryPickupFromWorldEntity(uint64_t worldItemEntityGuid);

		/// @brief 装備中アイテムをドロップします。
		/// @return 成功した場合 true
		bool TryDropEquippedItem();

		/// @brief 指定インデックスのアイテムをドロップします。
		/// @return 成功した場合 true
		bool TryDropItemByIndex(size_t index);

		/// @brief 指定インデックスのアイテムを装備します。
		/// @return 成功した場合 true
		bool TryEquipByIndex(size_t index);

		/// @brief 現在装備を解除します。
		/// @return 成功した場合 true
		bool TryUnequipCurrent();

		/// @brief コンポーネントが取り付けられた際の初期化です。
		void OnAttached() override;

		/// @brief コンポーネントが取り外された際の後始末です。
		void OnDetached() override;

		/// @brief 固定ティックごとに入力を消費して装備切り替えを更新します。
		void OnTick(float deltaTime) override;

		/// @brief インベントリは武器実行より先に入力を処理します。
		[[nodiscard]] TICK_GROUP GetTickGroup() const override {
			return TICK_GROUP::EARLY;
		}

		/// @brief シリアライズ時に使用する安定名を返します。
		[[nodiscard]] std::string_view GetStableName() const override;

		/// @brief 表示名を返します。
		[[nodiscard]] std::string_view GetComponentName() const override;

		/// @brief エディタアイコンを返します。
		[[nodiscard]] uint32_t GetIcon() const override;

#ifdef _DEBUG
		/// @brief エディタインスペクターを描画します。
		void DrawInspectorImGui() override;
#endif

		/// @brief JSONから設定を読み込みます。
		void Deserialize(const JsonReader& reader) override;

		/// @brief JSONへ設定を書き込みます。
		void Serialize(JsonWriter& writer) const override;

	private:
		/// @brief deterministic 入力を 1 ティック単位で保持するパケットです。
		struct DeterministicActionInputPacket {
			uint64_t                  tick        = 0;
			float                     stepSeconds = 0.0f;
			CharacterActionFrameInput input       = {};
		};

		/// @brief インベントリ 1 スロット分の保持データです。
		struct InventoryEntry {
			ItemInstance instance = {};
		};

		/// @brief 基本定義(拳など)を準備します。
		void EnsureCoreDefinitions();

		/// @brief 所持品が空の場合にフォールバック武器を補充します。
		void EnsureFallbackEntry();

		/// @brief サイクル入力を装備切り替えへ反映します。
		void ApplyCycleInput(const WeaponActionInput& input);

		/// @brief 相対インデックスで装備を切り替えます。
		[[nodiscard]] bool SelectRelativeEntry(int32_t delta);

		/// @brief 追加可能かどうかを検証します。
		[[nodiscard]] bool CanAcceptItem(const ItemInstance& instance) const;

		/// @brief 内部処理として拾得を実行します。
		[[nodiscard]] bool TryPickupItemInternal(
			const ItemInstance& instance,
			bool                emitEvents
		);

		/// @brief 装備切り替え内部処理です。
		[[nodiscard]] bool EquipIndexInternal(size_t index, bool emitEvents);

		/// @brief インデックス有効性を判定します。
		[[nodiscard]] bool IsValidIndex(size_t index) const;

		/// @brief インデックス指定でエントリを取得します。
		[[nodiscard]] const InventoryEntry* GetEntry(size_t index) const;

		/// @brief エントリがドロップ可能かを判定します。
		[[nodiscard]] bool CanDropEntry(const InventoryEntry& entry) const;

		/// @brief 指定位置のエントリを削除します。
		void RemoveEntryAt(size_t index);

		/// @brief インスタンスGUIDを採番します。
		[[nodiscard]] uint64_t AllocateInstanceGuid();

		/// @brief ワールド上のアイテムコンポーネントを解決します。
		[[nodiscard]] WorldItemComponent* ResolveWorldItemComponent(
			uint64_t worldItemEntityGuid
		) const;

		/// @brief イベント通知を Publish します。
		void PublishInventoryCue(
			std::string_view id,
			float            value  = 0.0f,
			float            value2 = 0.0f
		) const;

		/// @brief 参照可能なアイテム定義テーブルです。
		std::unordered_map<std::string, ItemDefinition> mDefinitions = {};
		/// @brief 現在の所持アイテム一覧です。
		std::vector<InventoryEntry> mEntries = {};
		/// @brief 現在装備中のスロットインデックスです。未装備は -1 です。
		int32_t mEquippedIndex = -1;
		/// @brief 次に採番するインスタンス GUID です。
		uint64_t mNextInstanceGuid = 1;
		/// @brief 所持可能な最大スロット数です。
		uint32_t mMaxEntries = 8;
		/// @brief 武器切替時に末尾/先頭ラップを許可するかです。
		bool mAllowCycleWrap = true;
		/// @brief 拾得時に自動装備するかです。
		bool mAutoEquipOnPickup = true;
		/// @brief このティックで消費したアクション入力です。
		CharacterActionFrameInput mActionFrameInput = {};
		/// @brief deterministic 入力をティック順で保持するリングバッファです。
		RingBuffer<
			DeterministicActionInputPacket,
			kDeterministicActionInputQueueCapacity
		>
		mDeterministicActionInputQueue;
	};
}
