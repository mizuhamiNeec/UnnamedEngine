#include "WorldItemComponent.h"

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/Icons.h"

namespace Unnamed {
	void WorldItemComponent::SetItemInstance(const ItemInstance& instance) {
		// 新しいインスタンスが設定されたら再度拾得可能に戻します。
		mItemInstance = instance;
		mAvailable    = true;
	}

	const ItemInstance& WorldItemComponent::GetItemInstance() const {
		return mItemInstance;
	}

	bool WorldItemComponent::IsAvailable() const {
		return mAvailable;
	}

	bool WorldItemComponent::TryTakeItemInstance(ItemInstance& outInstance) {
		// 取得済み、または空データなら失敗にします。
		if (!mAvailable || mItemInstance.itemId.empty()) {
			return false;
		}
		// 取り出し成功後は二重取得を防ぐため不可状態へ遷移します。
		outInstance = mItemInstance;
		mAvailable  = false;
		return true;
	}

	void WorldItemComponent::SetDestroyOwnerOnTake(const bool enabled) {
		mDestroyOwnerOnTake = enabled;
	}

	bool WorldItemComponent::GetDestroyOwnerOnTake() const {
		return mDestroyOwnerOnTake;
	}

	std::string_view WorldItemComponent::GetStableName() const {
		return "game.WorldItem";
	}

	std::string_view WorldItemComponent::GetComponentName() const {
		return "WorldItem";
	}

	uint32_t WorldItemComponent::GetIcon() const {
		return kIconLabel;
	}

	void WorldItemComponent::Deserialize(const JsonReader& reader) {
		// instance が壊れていても available / destroy フラグは復元します。
		(void)DeserializeItemInstance(reader["instance"], mItemInstance);
		mAvailable          = reader["available"].GetBool(mAvailable);
		mDestroyOwnerOnTake = reader["destroyOwnerOnTake"].GetBool(
			mDestroyOwnerOnTake
		);
	}

	void WorldItemComponent::Serialize(JsonWriter& writer) const {
		writer.Key("instance");
		writer.BeginObject();
		SerializeItemInstance(writer, mItemInstance);
		writer.EndObject();
		writer.Key("available");
		writer.Write(mAvailable);
		writer.Key("destroyOwnerOnTake");
		writer.Write(mDestroyOwnerOnTake);
	}

	REGISTER_COMPONENT(WorldItemComponent);
}
