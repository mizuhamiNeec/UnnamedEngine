#include "LoadedGameModule.h"

#include <pch.h>

#include "engine/Engine.h"
#include "engine/game/GameModuleRegistry.h"
#include "engine/game/GameRuntimeContext.h"
#include "engine/game/IDemoService.h"
#include "engine/game/IGameModule.h"
#include "engine/game/IGameWorldFactory.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	std::unique_ptr<LoadedGameModule> LoadedGameModule::Create(
		const GameModuleRegistry& registry,
		const std::string_view    requestedModuleName
	) {
		std::unique_ptr<IGameModule> gameModule = registry.Create(requestedModuleName);
		if (!gameModule) {
			return nullptr;
		}
		return std::unique_ptr<LoadedGameModule>(
			new LoadedGameModule(
				std::string(requestedModuleName),
				std::move(gameModule)
			)
		);
	}

	LoadedGameModule::LoadedGameModule(
		std::string                 requestedModuleName,
		std::unique_ptr<IGameModule> gameModule
	) : mRequestedModuleName(std::move(requestedModuleName)),
	    mGameModule(std::move(gameModule)),
	    mRuntimeContext(std::make_unique<GameRuntimeContext>()) {
		if (!mRuntimeContext || !mGameModule) {
			return;
		}

		mRuntimeContext->runtimeModuleName = mRequestedModuleName;
		mRuntimeContext->modulePaths = mGameModule->GetGameModulePaths();
		mRuntimeContext->defaultStartupScenePath =
			mGameModule->GetDefaultStartupScenePath();
		if (mRuntimeContext->defaultStartupScenePath.IsEmpty()) {
			mRuntimeContext->defaultStartupScenePath =
				mRuntimeContext->modulePaths.defaultStartupScene;
		}
		mRuntimeContext->defaultUiDocumentPath =
			mGameModule->GetDefaultUiDocumentPath();
	}

	LoadedGameModule::~LoadedGameModule() {
		UnregisterRuntimeContextService();
	}

	void LoadedGameModule::RegisterRuntimeContextService() {
		if (mRuntimeContextRegistered || !mRuntimeContext) {
			return;
		}
		ServiceLocator::Register<IGameModule>(mGameModule.get());
		ServiceLocator::Register<GameRuntimeContext>(mRuntimeContext.get());
		mRuntimeContextRegistered = true;
	}

	void LoadedGameModule::UnregisterRuntimeContextService() {
		if (!mRuntimeContextRegistered) {
			return;
		}
		ServiceLocator::Register<GameRuntimeContext>(nullptr);
		ServiceLocator::Register<IGameModule>(nullptr);
		mRuntimeContextRegistered = false;
	}

	bool LoadedGameModule::RegisterAndLoad(Engine& engine) {
		if (!mGameModule) {
			Error("Launcher", "LoadedGameModule has no IGameModule instance.");
			return false;
		}

		mGameModule->RegisterAssetTypes(engine);
		mGameModule->RegisterComponents(engine);
		mGameModule->RegisterSystems(engine);
		mGameModule->RegisterConsoleCommands(engine);
		mGameModule->OnLoad(engine);
		mLoaded = true;

		Msg(
			"Launcher",
			"GameModule loaded: runtimeModule='{}' gameModule='{}'.",
			mRequestedModuleName,
			mGameModule->GetName()
		);
		return true;
	}

	void LoadedGameModule::Unload(Engine& engine) {
		if (!mLoaded || !mGameModule) {
			return;
		}

		mGameModule->OnUnload(engine);
		mLoaded = false;
		Msg(
			"Launcher",
			"GameModule unloaded: runtimeModule='{}' gameModule='{}'.",
			mRequestedModuleName,
			mGameModule->GetName()
		);
	}

	IGameWorldFactory& LoadedGameModule::GetWorldFactory() const {
		return *mGameModule;
	}

	std::unique_ptr<IDemoService> LoadedGameModule::CreateDemoService() {
		if (!mGameModule) {
			return nullptr;
		}
		return mGameModule->CreateDemoService();
	}

	GameRuntimeContext& LoadedGameModule::GetRuntimeContext() {
		return *mRuntimeContext;
	}

	std::string LoadedGameModule::GetGameModuleName() const {
		if (!mGameModule) {
			return {};
		}
		return mGameModule->GetName();
	}

	const std::string& LoadedGameModule::GetRequestedModuleName() const {
		return mRequestedModuleName;
	}
}
