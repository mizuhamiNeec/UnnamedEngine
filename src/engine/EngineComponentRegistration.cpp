#include "EngineComponentRegistration.h"

#include "engine/ComponentRegistry.h"

#include "EngineComponentCatalog.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	void RegisterDefaultEngineComponents(ComponentRegistry& componentRegistry) {
		const auto RegisterIfMissing = [&](auto typeTag) {
			using T = decltype(typeTag)::type;
			const T                probe{};
			const std::string_view stableName = probe.GetStableName();
			if (componentRegistry.IsRegistered(stableName)) {
				return;
			}

			const bool registered = componentRegistry.Register(
				stableName,
				[]() -> std::unique_ptr<BaseComponent> {
					return std::make_unique<T>();
				},
				probe.GetComponentName()
			);
			if (!registered) {
				Warning(
					"EngineComponentRegistration",
					"Failed to register engine component '{}'.",
					stableName
				);
			}
		};

		ForEachEngineComponentType(RegisterIfMissing);
	}
}
