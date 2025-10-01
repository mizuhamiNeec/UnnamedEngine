#pragma once
#include <engine/public/gameframework/component/base/BaseComponent.h>
#include <engine/public/gameframework/world/UWorld.h>

namespace Unnamed {
	class WorldInstanceComponent : public BaseComponent {
	public:
		std::string             worldPath;
		std::unique_ptr<UWorld> subWorld;

		// BaseComponent
		void OnAttached() override;
		void OnDetached() override;

		void Serialize(JsonWriter& writer) const override;
		void Deserialize(const JsonReader& reader) override;

		bool LoadAndAtatch(UAssetManager* am, TransformComponent* parent);

		[[nodiscard]] std::string_view GetComponentName() const override;
	};
}
