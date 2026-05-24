#include <pch.h>

#include "ParkourGameModule.h"
#include "ParkourComponentRegistration.h"

#include "core/ComponentRegistry.h"
#include "engine/physics/core/Physics.h"
#include "engine/scene/Scene.h"
#include "engine/game/GamePathResolver.h"
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
			.gameRoot            = "./projects/ParkourGame",
			.contentRoot         = std::string(kParkourProjectContentRoot),
			.configRoot          = "./projects/ParkourGame/config",
			.defaultStartupScene = "scenes/title.json",
		};
	}

	std::string ParkourGameModule::GetDefaultStartupScenePath() const {
		return GetGameModulePaths().defaultStartupScene;
	}

	std::string ParkourGameModule::GetDefaultUiDocumentPath() const {
		return ResolveGameContentPath(
			GetGameModulePaths(),
			"ui/MainMenu.ui.json"
		);
	}

	std::unique_ptr<IGameModule> CreateParkourGameModule() {
		return std::make_unique<ParkourGameModule>();
	}
}
