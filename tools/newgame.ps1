param(
    [Parameter(Mandatory = $true)]
    [string]$Name,
    [string]$Alias = "",
    [string]$ProjectsRoot = ""
)

$ErrorActionPreference = "Stop"

function Write-TextFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Content
    )

    $dir = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    Set-Content -Path $Path -Value $Content -Encoding UTF8
}

function To-LowerSnake {
    param([string]$Value)
    return ($Value -replace "([a-z0-9])([A-Z])", '$1_$2').ToLowerInvariant()
}

$gameName = $Name.Trim()
if ([string]::IsNullOrWhiteSpace($gameName)) {
    throw "Name must not be empty."
}

if ([string]::IsNullOrWhiteSpace($Alias)) {
    $Alias = $gameName
}

$repoRoot = (Resolve-Path ".").Path
$resolvedProjectsRoot = ""
if ([string]::IsNullOrWhiteSpace($ProjectsRoot)) {
    $resolvedProjectsRoot = Join-Path $repoRoot "projects"
} else {
    $resolvedProjectsRoot = $ProjectsRoot
}
if (-not (Test-Path -LiteralPath $resolvedProjectsRoot -PathType Container)) {
    New-Item -ItemType Directory -Force -Path $resolvedProjectsRoot | Out-Null
}
$resolvedProjectsRoot = (Resolve-Path $resolvedProjectsRoot).Path

$gameRoot = Join-Path $resolvedProjectsRoot $gameName
$runtimeRoot = Join-Path $gameRoot "runtime/game/$(To-LowerSnake $gameName)"
$runtimeModuleRoot = Join-Path $runtimeRoot "runtime"
$componentsRoot = Join-Path $runtimeRoot "components"
$contentScenesRoot = Join-Path $gameRoot "content/scenes"
$configRoot = Join-Path $gameRoot "config"

$moduleName = "${gameName}GameModule"
$registrationName = "${gameName}ComponentRegistration"
$sampleComponentName = "${gameName}SampleComponent"
$sampleStableName = "$(To-LowerSnake $gameName).Sample"
$sampleDisplayName = "$gameName Sample"

$gameProfileJson = @"
{
	"schemaVersion": 1,
	"gameName": "$gameName",
	"aliases": [
		"$Alias"
	],
	"gameRoot": "./projects/$gameName",
	"contentRoot": "./projects/$gameName/content",
	"configRoot": "./projects/$gameName/config",
	"defaultStartupScene": "scenes/bootstrap.json"
}
"@

$bootstrapSceneJson = @"
{
  "version": 1,
  "name": "${gameName}Bootstrap",
  "entities": [
    {
      "active": true,
      "components": [
        {
          "active": true,
          "data": {
            "parentEntityGuid": 0,
            "position": [0.0, 0.0, 0.0],
            "rotation": [0.0, 0.0, 0.0, 1.0],
            "scale": [1.0, 1.0, 1.0]
          },
          "guid": 6001,
          "type": "engine.Transform"
        },
        {
          "active": true,
          "data": {},
          "guid": 6002,
          "type": "$sampleStableName"
        }
      ],
      "folderPath": "Bootstrap",
      "guid": 5001,
      "isEditorOnly": false,
      "name": "${gameName}SampleEntity",
      "visible": true
    }
  ]
}
"@

$moduleHeader = @"
#pragma once

#include "engine/game/IGameModule.h"

namespace Unnamed {
	/// @brief $gameName 向けの最小 GameModule 実装です。
	class $moduleName final : public IGameModule {
	public:
		/// @brief モジュールを初期化します。
		void Initialize(EngineServices& services) override;
		/// @brief Standalone 向けランタイムワールドを生成します。
		[[nodiscard]] std::unique_ptr<World> CreateRuntimeWorld(
			const WorldServices& services
		) override;
		/// @brief PIE 向けワールドを生成します。
		[[nodiscard]] std::unique_ptr<World> CreatePlayWorld(
			const WorldServices& services
		) override;
		/// @brief Demo サービス実装を生成します。
		[[nodiscard]] std::unique_ptr<IDemoService> CreateDemoService() override;
		/// @brief ゲーム固有コンポーネントを登録します。
		void RegisterGameComponents(ComponentRegistry& componentRegistry) override;
		/// @brief ゲーム名・ルート・既定シーン情報を返します。
		[[nodiscard]] GameModulePaths GetGameModulePaths() const override;
		/// @brief 起動時デフォルトシーンパスを返します。
		[[nodiscard]] std::string GetDefaultStartupScenePath() const override;
		/// @brief UI ドキュメントのデフォルトパスを返します。
		[[nodiscard]] std::string GetDefaultUiDocumentPath() const override;
	};

	/// @brief $gameName GameModule を生成します。
	[[nodiscard]] std::unique_ptr<IGameModule> Create${gameName}GameModule();
}
"@

$moduleCpp = @"
#include "$moduleName.h"
#include "$registrationName.h"

#include "engine/game/IDemoService.h"
#include "engine/physics/core/Physics.h"
#include "engine/scene/Scene.h"
#include "engine/world/World.h"

namespace Unnamed {
	void $moduleName::Initialize(EngineServices& services) {
		(void)services;
	}

	std::unique_ptr<World> $moduleName::CreateRuntimeWorld(
		const WorldServices& services
	) {
		auto world = std::make_unique<World>();
		world->SetServices(services);
		return world;
	}

	std::unique_ptr<World> $moduleName::CreatePlayWorld(
		const WorldServices& services
	) {
		auto world = std::make_unique<World>();
		world->SetServices(services);
		return world;
	}

	std::unique_ptr<IDemoService> $moduleName::CreateDemoService() {
		return nullptr;
	}

	void $moduleName::RegisterGameComponents(
		ComponentRegistry& componentRegistry
	) {
		Register${gameName}GameComponents(componentRegistry);
	}

	GameModulePaths $moduleName::GetGameModulePaths() const {
		return {
			.gameName            = "$gameName",
			.gameRoot            = "./projects/$gameName",
			.contentRoot         = "./projects/$gameName/content",
			.configRoot          = "./projects/$gameName/config",
			.defaultStartupScene = "scenes/bootstrap.json",
		};
	}

	std::string $moduleName::GetDefaultStartupScenePath() const {
		return GetGameModulePaths().defaultStartupScene;
	}

	std::string $moduleName::GetDefaultUiDocumentPath() const {
		return {};
	}

	std::unique_ptr<IGameModule> Create${gameName}GameModule() {
		return std::make_unique<$moduleName>();
	}
}
"@

$registrationHeader = @"
#pragma once

namespace Unnamed {
	class ComponentRegistry;

	/// @brief $gameName のコンポーネントを明示登録します。
	void Register${gameName}GameComponents(ComponentRegistry& componentRegistry);
}
"@

$registrationCpp = @"
#include "$registrationName.h"

#include "core/ComponentRegistry.h"
#include "engine/unnamed/subsystem/console/Log.h"

#include "game/$(To-LowerSnake $gameName)/components/$sampleComponentName.h"

namespace Unnamed {
	namespace {
		template <typename T>
		void RegisterComponentIfMissing(
			ComponentRegistry&      componentRegistry,
			const std::string_view stableName,
			const std::string_view displayName
		) {
			if (componentRegistry.IsRegistered(stableName)) {
				return;
			}

			const bool registered = componentRegistry.Register(
				stableName,
				[]() -> std::unique_ptr<BaseComponent> {
					return std::make_unique<T>();
				},
				displayName
			);
			if (!registered) {
				Warning(
					"$gameName Runtime",
					"Failed to register game component '{}'.",
					stableName
				);
			}
		}
	}

	void Register${gameName}GameComponents(ComponentRegistry& componentRegistry) {
		RegisterComponentIfMissing<$sampleComponentName>(
			componentRegistry,
			"$sampleStableName",
			"$sampleDisplayName"
		);
	}
}
"@

$componentHeader = @"
#pragma once

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	/// @brief $gameName の最小サンプル用コンポーネントです。
	class $sampleComponentName final : public BaseComponent {
	public:
		/// @brief Entity に取り付けられた直後に呼び出されます。
		void OnAttached() override;

		/// @brief シーン保存時に参照される安定名を返します。
		[[nodiscard]] std::string_view GetStableName() const override {
			return "$sampleStableName";
		}

		/// @brief エディタ表示用のコンポーネント名を返します。
		[[nodiscard]] std::string_view GetComponentName() const override {
			return "$sampleDisplayName";
		}

		/// @brief JSON からコンポーネント状態を復元します。
		void Deserialize(const JsonReader& reader) override;
		/// @brief JSON へコンポーネント状態を書き出します。
		void Serialize(JsonWriter& writer) const override;
	};
}
"@

$componentCpp = @"
#include "$sampleComponentName.h"

#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	void $sampleComponentName::OnAttached() {
		BaseComponent::OnAttached();
		const Entity* owner = GetOwner();
		const std::string_view ownerName = owner ? owner->GetName() : "<NoOwner>";
		DevMsg("$gameName", "$sampleComponentName attached to '{}'", ownerName);
	}

	void $sampleComponentName::Deserialize(const JsonReader& reader) {
		(void)reader;
	}

	void $sampleComponentName::Serialize(JsonWriter& writer) const {
		(void)writer;
	}
}
"@

$configReadme = @"
# $gameName Config

This folder contains runtime config files for $gameName.

- game_profile.json: runtime manifest consumed by GameModuleFactory.
- user.cfg: game-local archived/default convar values.
"@

$configUserCfg = @"
post_bloommipcount 8
asset_hotreloadpollinterval 0.125
r_vsync false
fps_max 0
"@

Write-TextFile -Path (Join-Path $configRoot "game_profile.json") -Content $gameProfileJson
Write-TextFile -Path (Join-Path $configRoot "README.md") -Content $configReadme
Write-TextFile -Path (Join-Path $configRoot "user.cfg") -Content $configUserCfg
Write-TextFile -Path (Join-Path $contentScenesRoot "bootstrap.json") -Content $bootstrapSceneJson

Write-TextFile -Path (Join-Path $runtimeModuleRoot "$moduleName.h") -Content $moduleHeader
Write-TextFile -Path (Join-Path $runtimeModuleRoot "$moduleName.cpp") -Content $moduleCpp
Write-TextFile -Path (Join-Path $runtimeModuleRoot "$registrationName.h") -Content $registrationHeader
Write-TextFile -Path (Join-Path $runtimeModuleRoot "$registrationName.cpp") -Content $registrationCpp
Write-TextFile -Path (Join-Path $componentsRoot "$sampleComponentName.h") -Content $componentHeader
Write-TextFile -Path (Join-Path $componentsRoot "$sampleComponentName.cpp") -Content $componentCpp

Write-Host "Created game template for '$gameName' under $gameRoot."
Write-Host "Next steps:"
Write-Host "  1) Add runtime/app wiring to premake5.lua and src/app/*Main.cpp."
Write-Host "  2) Run .\\generateallprojects.ps1"
Write-Host "  3) Build Debug: msbuild Unnamed.slnx /m /p:Configuration=Debug /p:Platform=x64"
