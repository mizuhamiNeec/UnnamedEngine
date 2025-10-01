#include "WorldInstanceComponent.h"

#include <core/json/JsonReader.h>
#include <core/json/JsonWriter.h>

namespace Unnamed {
	void WorldInstanceComponent::OnAttached() {
		BaseComponent::OnAttached();
	}

	void WorldInstanceComponent::OnDetached() {
		subWorld.reset();
	}

	void WorldInstanceComponent::Serialize(JsonWriter& writer) const {
		writer.Key("world");
		writer.Write(worldPath);
	}

	void WorldInstanceComponent::Deserialize(const JsonReader& reader) {
		if (reader.Has("world")) {
			worldPath = reader["world"].GetString();
		}
	}

	bool WorldInstanceComponent::LoadAndAtatch(
		UAssetManager* am, [[maybe_unused]] TransformComponent* parent
	) {
		if (worldPath.empty()) { return false; }
		auto sub = std::make_unique<UWorld>();
		if (!sub->LoadFromJson(worldPath, am)) { return false; }
		subWorld = std::move(sub);
		return true;
	}

	std::string_view WorldInstanceComponent::GetComponentName() const {
		return "WorldInstance";
	}
}
