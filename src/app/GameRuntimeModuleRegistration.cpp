#include <pch.h>

#include "GameRuntimeModuleRegistration.h"

#include "engine/game/GameModuleRegistry.h"

#ifdef UNNAMED_WITH_PARKOUR_RUNTIME
#include "game/parkour/runtime/ParkourGameModule.h"
#endif

namespace Unnamed {
	void RegisterBuiltInGameModules(GameModuleRegistry& registry) {
#ifdef UNNAMED_WITH_PARKOUR_RUNTIME
		(void)registry.RegisterFactory("ParkourGameRuntime", &CreateParkourGameModule);
		(void)registry.RegisterFactory("ParkourGame", &CreateParkourGameModule);
		(void)registry.RegisterFactory("Parkour", &CreateParkourGameModule);
#else
		Error(
			"Launcher",
			"Parkour runtime is not linked. Re-generate with Parkour runtime enabled."
		);
#endif
	}
}
