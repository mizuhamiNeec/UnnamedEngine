#include "WorldInstanceComponent.h"

#include <core/json/JsonReader.h>
#include <core/json/JsonWriter.h>

namespace Unnamed {
	/// @brief エンティティにアタッチされたときの処理。
	void WorldInstanceComponent::OnAttached() {
		BaseComponent::OnAttached();
	}

	/// @brief エンティティからデタッチされたときの処理。
	void WorldInstanceComponent::OnDetached() {
		subWorld.reset();
	}

	/// @brief コンポーネントのシリアライズ処理。
	/// @param writer シリアライズ先のJsonWriter。
	void WorldInstanceComponent::Serialize(JsonWriter& writer) const {
		writer.Key("world");
		writer.Write(worldPath);
	}

	/// @brief コンポーネントのデシリアライズ処理。
	/// @param reader デシリアライズ元のJsonReader。
	void WorldInstanceComponent::Deserialize(const JsonReader& reader) {
		if (reader.Has("world")) {
			worldPath = reader["world"].GetString();
		}
	}

	/// @brief サブワールドをロードしてアタッチする。
	bool WorldInstanceComponent::LoadAndAttach(
		UAssetManager* am, [[maybe_unused]] TransformComponent* parent
	) {
		if (worldPath.empty()) { return false; }
		auto sub = std::make_unique<UWorld>();
		if (!sub->LoadFromJson(worldPath, am)) { return false; }
		subWorld = std::move(sub);
		return true;
	}

	/// @brief コンポーネント名を取得する。
	/// @return コンポーネント名。
	std::string_view WorldInstanceComponent::GetComponentName() const {
		return "WorldInstance";
	}
}
