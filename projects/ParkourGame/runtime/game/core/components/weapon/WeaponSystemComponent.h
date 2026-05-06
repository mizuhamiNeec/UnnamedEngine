#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>

#include "core/containers/RingBuffer.h"
#include "core/math/Vec3.h"

#include "engine/unnamed/framework/components/base/BaseComponent.h"

#include "game/core/input/CharacterActionFrameInput.h"
#include "game/core/item/ItemTypes.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;
	class World;
	class Entity;
	class InventorySystemComponent;

	/// @brief 武器モジュールの実行時状態です。
	struct WeaponModuleRuntime {
		float    cooldownRemainingSec = 0.0f;
		uint64_t executionCount       = 0;
	};

	/// @brief 武器モジュールが参照する実行コンテキストです。
	struct WeaponActionRuntimeContext {
		uint64_t simulationTick = 0;
		float    stepSeconds    = 0.0f;
		Vec3     origin         = Vec3::zero;
		Vec3     forward        = Vec3::forward;
		Vec3     right          = Vec3::right;
		Vec3     up             = Vec3::up;
		World*   world          = nullptr;
		Entity*  owner          = nullptr;
		bool     drawDebugTrace = false;
		bool     verboseLogging = false;
	};

	/// @brief 武器挙動差し替え用モジュールIFです。
	class IWeaponActionModule {
	public:
		virtual ~IWeaponActionModule() = default;

		/// @brief モードIDを返します。
		[[nodiscard]] virtual std::string_view GetModeId() const = 0;

		/// @brief このモジュールが選択されたときに呼ばれます。
		virtual void OnSelected(
			const WeaponActionRuntimeContext& context,
			const ItemDefinition&             itemDefinition,
			const WeaponItemDefinition&       weaponDefinition,
			WeaponModuleRuntime&              runtime
		);

		/// @brief このモジュールが非選択になったときに呼ばれます。
		virtual void OnDeselected(
			const WeaponActionRuntimeContext& context,
			const ItemDefinition&             itemDefinition,
			const WeaponItemDefinition&       weaponDefinition,
			WeaponModuleRuntime&              runtime
		);

		/// @brief 固定ティックで入力を消費して挙動を実行します。
		virtual void Tick(
			const WeaponActionRuntimeContext& context,
			const WeaponActionInput&          input,
			const ItemDefinition&             itemDefinition,
			const WeaponItemDefinition&       weaponDefinition,
			WeaponModuleRuntime&              runtime
		) = 0;
	};

	/// @brief 武器実行専任のコンポーネントです。
	/// @details 所持/装備切替は InventorySystemComponent が担当し、このクラスは実行のみを担当します。
	class WeaponSystemComponent final : public BaseComponent,
	                                    public ICharacterActionInputReceiver {
	public:
		/// @brief deterministic アクション入力キューの最大保持件数です。
		static constexpr size_t kDeterministicActionInputQueueCapacity = 128;

		/// @brief 破棄時に内部ランタイムを解放します。
		~WeaponSystemComponent() override;

		/// @brief deterministic アクション入力をキューに追加します。
		void EnqueueDeterministicActionInput(
			uint64_t                         tick,
			float                            stepSeconds,
			const CharacterActionFrameInput& input
		) override;

		/// @brief deterministic アクション入力キューをクリアします。
		void ClearDeterministicActionInputQueue() override;

		/// @brief 現在のアクション入力を取得します。
		[[nodiscard]] const CharacterActionFrameInput& GetActionFrameInput()
		const;

		/// @brief コンポーネントがエンティティに取り付けられた時の初期化です。
		void OnAttached() override;

		/// @brief コンポーネントがエンティティから取り外された時の終了処理です。
		void OnDetached() override;

		/// @brief 固定ティックごとに入力を消費して武器挙動を更新します。
		void OnTick(float deltaTime) override;

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
		/// @brief deterministic 入力を 1 ティック分保持するパケットです。
		struct DeterministicActionInputPacket {
			uint64_t                  tick        = 0;
			float                     stepSeconds = 0.0f;
			CharacterActionFrameInput input       = {};
		};

		/// @brief アイテムインスタンスごとの武器モジュール実行状態です。
		struct WeaponRuntimeSlot {
			std::string                          modeId  = {};
			WeaponModuleRuntime                  runtime = {};
			std::unique_ptr<IWeaponActionModule> module  = {};
		};

		/// @brief 現在のカメラ/所有者情報から実行コンテキストを構築します。
		[[nodiscard]] WeaponActionRuntimeContext BuildRuntimeContext(
			uint64_t simulationTick,
			float    stepSeconds
		) const;

		/// @brief 装備中武器定義を解決します。
		[[nodiscard]] bool TryResolveEquippedWeapon(
			const InventorySystemComponent*& outInventory,
			const ItemDefinition*&           outItemDefinition,
			const ItemInstance*&             outItemInstance,
			const WeaponItemDefinition*&     outWeaponDefinition
		) const;

		/// @brief 装備中武器を1ティック実行します。
		void TickEquippedWeapon(
			const WeaponActionRuntimeContext& context,
			const WeaponActionInput&          input,
			const ItemDefinition&             itemDefinition,
			const ItemInstance&               itemInstance,
			const WeaponItemDefinition&       weaponDefinition
		);

		/// @brief 全武器ランタイムのクールダウンを進めます。
		void TickCooldowns(float deltaTime);

		/// @brief このティックで消費したアクション入力です。
		CharacterActionFrameInput mActionFrameInput = {};
		/// @brief deterministic 入力をティック順で保持するリングバッファです。
		RingBuffer<
			DeterministicActionInputPacket,
			kDeterministicActionInputQueueCapacity
		>
		mDeterministicActionInputQueue;
		/// @brief アイテムインスタンス GUID ごとの武器ランタイム状態です。
		std::unordered_map<uint64_t, WeaponRuntimeSlot> mRuntimeByItemInstance =
			{};
		/// @brief 前回実行対象だった装備インスタンス GUID です。
		uint64_t mLastEquippedInstanceGuid = 0;
		/// @brief 詳細ログを出すかです。
		bool mVerboseLogging = false;
		/// @brief 武器トレースのデバッグ描画を出すかです。
		bool mDrawDebugTrace = true;
	};
}
