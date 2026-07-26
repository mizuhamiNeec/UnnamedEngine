#include "ParkourComponentRegistration.h"

#include "engine/ComponentRegistry.h"

#include "engine/unnamed/framework/components/DirectionalLightComponent.h"
#include "engine/unnamed/framework/components/SkyLightComponent.h"
#include "engine/unnamed/subsystem/console/Log.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"
#include "game/core/components/AudioFxControllerComponent.h"
#include "game/core/components/CameraFxControllerComponent.h"
#include "game/core/components/CameraRotatorComponent.h"
#include "game/core/components/ViewmodelSway.h"
#include "game/core/components/character/GameMovementComponent.h"
#include "game/core/components/character/base/BaseCharacterComponent.h"
#include "game/core/components/character/state/GameMovementStateMachine.h"
#include "game/core/components/common/PatrolPointComponent.h"
#include "game/core/components/common/RotatorComponent.h"
#include "game/core/components/controller/PlayerCharacterController.h"
#include "game/core/components/controller/base/BaseCharacterController.h"
#include "game/core/components/inventory/InventorySystemComponent.h"
#include "game/core/components/inventory/WorldItemComponent.h"
#include "game/core/components/presentation/EventPresentationComponent.h"
#include "game/core/components/weapon/WeaponSystemComponent.h"
#include "game/core/script/ConsoleScriptComponent.h"
#include "game/parkour/components/character/ParkourMovementComponent.h"
#include "game/parkour/components/course/CourseProgressComponent.h"
#include "game/parkour/components/course/CourseProgressHudComponent.h"
#include "game/parkour/components/course/CourseResultFlowComponent.h"
#include "game/parkour/components/cutscene/GameStartCutsceneComponent.h"
#include "game/parkour/components/title/TitleFlowComponent.h"
#include "game/parkour/components/trigger/CheckpointComponent.h"
#include "game/parkour/components/trigger/GoalComponent.h"
#include "game/parkour/components/trigger/JumpPadComponent.h"
#include "game/parkour/components/trigger/SpeedBoostAreaComponent.h"

namespace Unnamed {
	namespace {
		template <typename T>
		void RegisterComponentIfMissing(
			ComponentRegistry&     componentRegistry,
			const std::string_view stableName,
			const std::string_view displayName
		) {
			if (componentRegistry.IsRegistered(stableName)) {
				return;
			}

			const bool registered = componentRegistry.RegisterTyped<T>(
				stableName, displayName
			);
			if (!registered) {
				Warning(
					"ParkourRuntime",
					"Failed to register game component '{}'.",
					stableName
				);
			}
		}
	}

	void RegisterParkourGameComponents(ComponentRegistry& componentRegistry) {
		RegisterComponentIfMissing<AudioFxControllerComponent>(
			componentRegistry, "game.AudioFxController", "AudioFxController"
		);
		RegisterComponentIfMissing<CameraFxControllerComponent>(
			componentRegistry, "game.CameraFxController", "CameraFxController"
		);
		RegisterComponentIfMissing<CameraRotatorComponent>(
			componentRegistry, "game.CameraRotator", "CameraRotator"
		);
		RegisterComponentIfMissing<ViewmodelSway>(
			componentRegistry, "game.ViewmodelSway", "ViewmodelSway"
		);
		RegisterComponentIfMissing<BaseCharacterComponent>(
			componentRegistry,
			"game.BaseCharacterComponent",
			"BaseCharacterComponent"
		);
		RegisterComponentIfMissing<GameMovementComponent>(
			componentRegistry, "game.GameMovement", "GameMovement"
		);
		RegisterComponentIfMissing<BaseCharacterController>(
			componentRegistry,
			"game.BaseCharacterController",
			"BaseCharacterController"
		);
		RegisterComponentIfMissing<PlayerCharacterController>(
			componentRegistry,
			"game.PlayerCharacterController",
			"PlayerCharacterController"
		);
		RegisterComponentIfMissing<InventorySystemComponent>(
			componentRegistry, "game.InventorySystem", "InventorySystem"
		);
		RegisterComponentIfMissing<WorldItemComponent>(
			componentRegistry, "game.WorldItem", "WorldItem"
		);
		RegisterEventPresentationComponent(componentRegistry);
		RegisterComponentIfMissing<WeaponSystemComponent>(
			componentRegistry, "game.WeaponSystem", "WeaponSystem"
		);
		RegisterComponentIfMissing<PatrolPointComponent>(
			componentRegistry, "game.PatrolPoint", "PatrolPoint"
		);
		RegisterComponentIfMissing<RotatorComponent>(
			componentRegistry, "game.Rotator", "Rotator"
		);
		RegisterComponentIfMissing<ConsoleScriptComponent>(
			componentRegistry, "engine.ScriptComponent", "ScriptComponent"
		);
		RegisterComponentIfMissing<ParkourMovementComponent>(
			componentRegistry, "parkour.ParkourMovement", "ParkourMovement"
		);
		RegisterComponentIfMissing<CourseProgressComponent>(
			componentRegistry, "parkour.CourseProgress", "CourseProgress"
		);
		RegisterComponentIfMissing<CourseProgressHudComponent>(
			componentRegistry, "parkour.CourseProgressHud", "CourseProgressHud"
		);
		RegisterComponentIfMissing<CourseResultFlowComponent>(
			componentRegistry, "parkour.CourseResultFlow", "CourseResultFlow"
		);
		RegisterComponentIfMissing<GameStartCutsceneComponent>(
			componentRegistry, "parkour.GameStartCutscene", "GameStartCutscene"
		);
		RegisterComponentIfMissing<TitleFlowComponent>(
			componentRegistry, "parkour.TitleFlow", "TitleFlow"
		);
		RegisterComponentIfMissing<CheckpointComponent>(
			componentRegistry, "parkour.Checkpoint", "Checkpoint"
		);
		RegisterComponentIfMissing<GoalComponent>(
			componentRegistry, "parkour.Goal", "Goal"
		);
		RegisterComponentIfMissing<JumpPadComponent>(
			componentRegistry, "parkour.JumpPad", "JumpPad"
		);
		RegisterComponentIfMissing<SpeedBoostAreaComponent>(
			componentRegistry, "parkour.SpeedBoostArea", "SpeedBoostArea"
		);
		RegisterComponentIfMissing<SkyLightComponent>(
			componentRegistry, "engine.SkyLight", "SkyLight"
		);
		RegisterComponentIfMissing<DirectionalLightComponent>(
			componentRegistry, "engine.DirectionalLight", "DirectionalLight"
		);
	}
}
