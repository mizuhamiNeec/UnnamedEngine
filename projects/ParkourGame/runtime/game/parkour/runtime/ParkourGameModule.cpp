#include <pch.h>
#include "core/filesystem/Path.h"

#include "ParkourGameModule.h"
#include "ParkourComponentRegistration.h"

#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/scene/Scene.h"

#include "engine/ComponentRegistry.h"
#include "game/core/replay/DemoManager.h"
#include "game/parkour/runtime/ParkourGameWorld.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel = "ParkourGameModule";
		constexpr std::string_view kParkourProjectContentRoot =
			"./projects/ParkourGame/content";
	}

	std::string ParkourGameModule::GetName() const {
		return "ParkourGameModule";
	}

	void ParkourGameModule::OnLoad(Engine& engine) {
		(void)engine;
		Msg(kChannel, "OnLoad completed.");
	}

	void ParkourGameModule::OnUnload(Engine& engine) {
		(void)engine;
		Msg(kChannel, "OnUnload completed.");
	}

	void ParkourGameModule::RegisterComponents(Engine& engine) {
		(void)engine;
		RegisterParkourGameComponents(ComponentRegistry::Get());
		Msg(kChannel, "Registered game components.");
	}

	void ParkourGameModule::RegisterSystems(Engine& engine) {
		(void)engine;
		DevMsg(kChannel, "RegisterSystems is currently empty.");
	}

	void ParkourGameModule::RegisterConsoleCommands(Engine& engine) {
		(void)engine;
		DevMsg(kChannel, "RegisterConsoleCommands is currently empty.");
	}

	void ParkourGameModule::RegisterAssetTypes(Engine& engine) {
		(void)engine;
		DevMsg(kChannel, "RegisterAssetTypes is currently empty.");
	}

	void ParkourGameModule::Initialize(EngineServices& services) {
		// 現時点の Parkour モジュールでは初期化フックは予約のみです。
		(void)services;
	}

	std::unique_ptr<World> ParkourGameModule::CreateRuntimeWorld(
		const WorldServices& services
	) {
		auto world = std::make_unique<ParkourGameWorld>();
		world->SetServices(services);
		return world;
	}

	std::unique_ptr<World> ParkourGameModule::CreatePlayWorld(
		const WorldServices& services
	) {
		auto world = std::make_unique<ParkourGameWorld>();
		world->SetServices(services);
		return world;
	}

	std::unique_ptr<IDemoService> ParkourGameModule::CreateDemoService() {
		return std::make_unique<DemoManager>();
	}

	void ParkourGameModule::RegisterGameComponents(
		ComponentRegistry& componentRegistry
	) {
		RegisterParkourGameComponents(componentRegistry);
	}

	GameModulePaths ParkourGameModule::GetGameModulePaths() const {
		return {
			.gameName            = "Parkour",
			.gameRoot            = Path("./projects/ParkourGame"),
			.contentRoot         = Path(kParkourProjectContentRoot),
			.configRoot          = Path("./projects/ParkourGame/config"),
			.defaultStartupScene = VirtualPath::ParseOrThrow(
				"scenes/title.json"),
		};
	}

	std::optional<VirtualPath> ParkourGameModule::GetDefaultUiDocument() const {
		return VirtualPath::ParseContentReference("ui/MainMenu.ui.json");
	}

	std::unique_ptr<IGameModule> CreateParkourGameModule() {
		return std::make_unique<ParkourGameModule>();
	}
}
