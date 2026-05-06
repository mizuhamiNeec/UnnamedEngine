#include "GameMovementComponent.h"

#include <algorithm>
#include <array>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "../controller/PlayerCharacterController.h"

#include "base/BaseCharacterComponent.h"

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/string/StrUtil.h"

#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/world/GameplayCueBus.h"
#include "engine/world/World.h"
#include "engine/game/IDemoService.h"

#include "game/core/collision/kinematic/BoxKinematicCollisionResolver.h"
#include "game/core/components/character/state/GameMovementStateMachine.h"
#include "game/core/replay/ReplayHash.h"

#include "state/ability/CoreMovementAbilities.h"
#include "state/mode/AirMovementMode.h"
#include "state/mode/GroundMovementMode.h"
#include "state/mode/NoclipMovementMode.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] const char* ToStringRecoverReason(
			const BoxKinematicCollisionResolver::RECOVER_REASON reason
		) {
			switch (reason) {
				case BoxKinematicCollisionResolver::RECOVER_REASON::SLIDE_START
				: return "slide_start";
				case
				BoxKinematicCollisionResolver::RECOVER_REASON::HIT_START_SOLID
				: return "hit_start_solid";
				case
				BoxKinematicCollisionResolver::RECOVER_REASON::HIT_ALL_SOLID
				: return "hit_allsolid";
				case
				BoxKinematicCollisionResolver::RECOVER_REASON::END_POSITION_OVERLAP
				: return "end_position_overlap";
				default: return "none";
			}
		}
	}

	GameMovementComponent::~GameMovementComponent() = default;

	void GameMovementComponent::SimulateStep(
		TransformComponent* transform, const MovementFrameInput& input,
		const float         stepSeconds
	) {
		if (!transform || !mCollisionResolver || !mStateMachine) {
			return;
		}
		mCollisionDebugLogCooldownSec = std::max(
			0.0f, mCollisionDebugLogCooldownSec - stepSeconds
		);

		const bool        wasGrounded            = mSupportCache.grounded;
		const float       preStepVerticalSpeedHu = Math::MtoH(mVelocity.y);
		const std::string previousStateName      = StrUtil::ToLowerCase(
			ResolvePresentationStateName()
		);

		mCurrentModeId = mStateMachine->GetCurrentModeId();
		mActiveAbilityMask = mStateMachine->GetActiveAbilityMask();
		MovementContext context = {};
		context.input = input;
		context.transform = transform;
		context.velocity = mVelocity;
		context.resolver = mCollisionResolver.get();
		context.halfExtents = mBoxHalfExtents;
		context.isGrounded = mSupportCache.grounded;
		context.supportEntityGuid = mSupportCache.supportEntityGuid;
		context.supportLinearVelocity = mSupportCache.supportLinearVelocity;
		context.supportStepDelta = mSupportCache.supportStepDelta;
		context.jumpSnapDisableRemaining = mJumpSnapDisableRemaining;
		context.defaultAirMode = GetAirModeForTransitions();
		context.movementComponent = this;
		context.capabilitySet = &mCapabilitySet;
		context.modeState.currentMode = mCurrentModeId;
		context.modeState.previousMode = mCurrentModeId;
		context.modeState.changedThisTick = false;
		context.activeAbilityMask = mActiveAbilityMask;

		UpdateCollisionHull(transform);
		mStateMachine->Tick(context, stepSeconds);
		mCurrentModeId            = mStateMachine->GetCurrentModeId();
		mActiveAbilityMask        = mStateMachine->GetActiveAbilityMask();
		mVelocity                 = context.velocity;
		mGrounded                 = context.isGrounded;
		mJumpSnapDisableRemaining = std::max(
			0.0f,
			context.jumpSnapDisableRemaining
		);

		const std::string currentStateName = StrUtil::ToLowerCase(
			ResolvePresentationStateName()
		);

		if (
			!previousStateName.empty() &&
			previousStateName != currentStateName
		) {
			// movement.state.exit.* payload contract:
			// value/value2 are currently unused (default 0).
			PublishCue("movement.state.exit." + previousStateName);
			if (!currentStateName.empty()) {
				// movement.state.enter.* payload contract:
				// value/value2 are currently unused (default 0).
				PublishCue("movement.state.enter." + currentStateName);
			}
		}

		if (
			wasGrounded &&
			!context.isGrounded &&
			input.jumpPressed &&
			context.velocity.y > 0.0f
		) {
			// movement.jump: 現状は payload 未使用（value/value2 は 0）。
			PublishCue("movement.jump");
		}

		if (!wasGrounded && context.isGrounded) {
			const float downSpeedHu  = std::max(0.0f, -preStepVerticalSpeedHu);
			const float landStrength = std::clamp(
				downSpeedHu / 400.0f, 0.0f, 200.0f
			);
			// movement.land payload contract:
			// value  = normalized land strength [0..200]
			// value2 = impactSpeed (downward speed in HU/s, >= 0)
			// named:
			//   payload.landStrength
			//   payload.impactSpeedHuPerSec
			PublishCue("movement.land", landStrength, downSpeedHu);
		}

		OnAfterCoreCueDispatch(
			previousStateName,
			currentStateName,
			input,
			wasGrounded,
			context.isGrounded,
			stepSeconds
		);

		if (context.isGrounded) {
			mSupportCache.grounded              = true;
			mSupportCache.supportEntityGuid     = context.supportEntityGuid;
			mSupportCache.supportLinearVelocity = Vec3::zero;
			mSupportCache.supportStepDelta      = Vec3::zero;
		} else {
			mSupportCache.grounded              = false;
			mSupportCache.supportEntityGuid     = 0;
			mSupportCache.supportLinearVelocity = Vec3::zero;
			mSupportCache.supportStepDelta      = Vec3::zero;
		}
	}

	void GameMovementComponent::OnAttached() {
		BaseCharacterComponent::OnAttached();

		mInput   = GetInputSystem();
		mConsole = GetConsoleSystem();

		mStateMachine = std::make_unique<GameMovementStateMachine>();
		mStateMachine->Init(mConsole);

		RegisterMovementModes(*mStateMachine);
		RegisterMovementAbilities(*mStateMachine);
		mStateMachine->SetInitialMode(GetInitialMode());
		mCurrentModeId     = mStateMachine->GetCurrentModeId();
		mActiveAbilityMask = mStateMachine->GetActiveAbilityMask();

		auto* physicsEngine = GetWorld() ?
			                      &GetWorld()->GetPhysicsEngine() :
			                      nullptr;
		mCollisionResolver = std::make_unique<BoxKinematicCollisionResolver>(
			physicsEngine
		);

		mSupportCache             = {};
		mJumpSnapDisableRemaining = 0.0f;
	}

	void GameMovementComponent::PrePhysicsTick(const float deltaTime) {
		BaseCharacterComponent::PrePhysicsTick(deltaTime);
	}

	void GameMovementComponent::OnTick(const float deltaTime) {
		TransformComponent* transform = GetTransform();
		if (!transform) {
			Error(
				GetComponentName(), "TransformComponentが見つからないため、移動できません。"
			);
			return;
		}

		DeterministicInputPacket deterministicPacket = {};
		if (TryDequeueDeterministicInput(deterministicPacket)) {
			const float stepSeconds = std::max(
				deterministicPacket.stepSeconds,
				1.0e-6f
			);

			mSupportCache.supportLinearVelocity = Vec3::zero;
			mSupportCache.supportStepDelta      = Vec3::zero;

			UpdateCollisionHull(transform);
			mMoveFrameInput = deterministicPacket.input;
			SimulateStep(transform, deterministicPacket.input, stepSeconds);

			if (Entity* owner = GetOwner()) {
				if (auto* demoService = GetDemoService()) {
					demoService->RecordOrVerifySnapshot(
						deterministicPacket.tick,
						*owner
					);
				}
			}

			transform->OnTick(0.0f);
			return;
		}

		// PlayerCharacterController が接続されている場合は、
		// deterministic 入力キューのみで移動更新する。
		// キュー空フレームで旧フレーム依存フォールバックへ入ると
		// FPS によって移動量が変わってしまうため、ここで抑止する。
		if (const Entity* owner = GetOwner()) {
			if (owner->GetComponent<PlayerCharacterController>() != nullptr) {
				transform->OnTick(0.0f);
				return;
			}
		}

		const float stepSec = std::max(deltaTime, 1.0e-6f);

		mSupportCache.supportLinearVelocity = Vec3::zero;
		mSupportCache.supportStepDelta      = Vec3::zero;

		UpdateCollisionHull(transform);
		SimulateStep(transform, mMoveFrameInput, stepSec);

		// GameMovement は Transform の TickGroup より後で位置を書き換えるため、
		// 同フレーム描画での1フレーム遅延を防ぐためにここで行列を確定する。
		transform->OnTick(0.0f);
	}

	void GameMovementComponent::PostPhysicsTick(float) {
		TransformComponent* transform = GetTransform();
		if (!transform) {
			Error(
				GetComponentName(), "TransformComponentが見つからないため、移動できません。"
			);
			return;
		}
		auto& worldDebugDraw = GetWorld()->GetDebugDraw();

		// デバッグ描画: 現在の速度を矢印で表示
		worldDebugDraw.DrawArrow(
			transform->GetPosition(),
			mVelocity * 0.25f, // 普通に出したら長いので縮める
			Vec4::yellow,
			0.125f
		);

		const MovementContext context = {
			.input = mMoveFrameInput,
		};

		if (!mStateMachine) {
			return;
		}

		const Vec3 right   = context.input.right.Normalized();
		Vec3       forward = context.input.forward.Normalized();
		forward.y          = 0.0f;
		forward.Normalize();
		Vec3 wishDir =
			right * context.input.moveAxis.x +
			forward * context.input.moveAxis.z;
		wishDir.y = 0.0f;

		// デバッグ描画: 移動方向をレイで表示
		worldDebugDraw.DrawRay(
			transform->GetPosition(),
			wishDir.Normalized(),
			Vec4::white
		);

		// デバッグ描画: 当たり判定のボックスを表示
		worldDebugDraw.DrawBox(
			transform->GetPosition(),
			transform->GetRotation(),
			mBoxHalfExtents * 2.0f,
			Vec4::cyan
		);

		const bool drawCollisionDebug = mConsole &&
		                                mConsole->GetConVarValueOr(
			                                "sv_move_collision_debugdraw", false
		                                );
		const bool logCollisionDebug = mConsole &&
		                               mConsole->GetConVarValueOr(
			                               "sv_move_collision_debuglog", false
		                               );
		auto* boxResolver = dynamic_cast<BoxKinematicCollisionResolver*>(
			mCollisionResolver.get()
		);
		if (!boxResolver) {
			return;
		}

		const BoxKinematicCollisionResolver::CollisionDebugState& debugState =
			boxResolver->GetCollisionDebugState();

		if (drawCollisionDebug && debugState.hasBlockingHit) {
			const Vec4 castColor = Vec4::orange;
			const Vec4 hitColor  = Vec4::red;

			// 衝突スイープの経路とヒット面法線を可視化します。
			worldDebugDraw.DrawArrow(
				debugState.blockingCastStart,
				debugState.blockingCastDir * debugState.blockingCastLength,
				castColor,
				Math::HtoM(4.0f)
			);
			worldDebugDraw.DrawSphere(
				debugState.blockingHit.pos,
				Quaternion::identity,
				Math::HtoM(2.0f),
				hitColor,
				10
			);
			worldDebugDraw.DrawArrow(
				debugState.blockingHit.pos,
				debugState.blockingHit.normal * Math::HtoM(14.0f),
				hitColor,
				Math::HtoM(2.0f)
			);
		}
		if (drawCollisionDebug && debugState.recoverAttempted) {
			const Vec4 recoverColor = debugState.recoverSucceeded ?
				                          Vec4(0.2f, 1.0f, 0.4f, 1.0f) :
				                          Vec4(1.0f, 0.1f, 0.1f, 1.0f);

			worldDebugDraw.DrawArrow(
				debugState.recoverStart,
				debugState.recoverEnd - debugState.recoverStart,
				recoverColor,
				Math::HtoM(3.0f)
			);
			for (int i = 0; i < debugState.recoverOverlapCount; ++i) {
				const Physics::Hit& overlap = debugState.recoverOverlaps[
					static_cast<size_t>(i)
				];
				const float overlapLen = std::clamp(
					overlap.depth,
					Math::HtoM(1.0f),
					Math::HtoM(16.0f)
				);
				worldDebugDraw.DrawArrow(
					debugState.recoverStart,
					overlap.normal * overlapLen,
					Vec4::yellow,
					Math::HtoM(1.5f)
				);
			}
		}

		if (
			logCollisionDebug &&
			debugState.hasBlockingHit &&
			mCollisionDebugLogCooldownSec <= 0.0f
		) {
			const float logInterval = mConsole->GetConVarValueOr(
				"sv_move_collision_debuglog_interval", 0.15f
			);
			const Physics::Hit& hit = debugState.blockingHit;
			Msg(
				"GameMovement",
				"CollisionDebug guid={} tri={} toi={:.4f} depth={:.5f} startSolid={} allSolid={} normal=({:.3f},{:.3f},{:.3f}) hitPos=({:.3f},{:.3f},{:.3f}) recover(reason={} ok={} overlaps={})",
				static_cast<unsigned long long>(hit.hitEntityGuid),
				hit.triIndex,
				hit.toi,
				hit.depth,
				hit.startSolid,
				hit.allsolid,
				hit.normal.x,
				hit.normal.y,
				hit.normal.z,
				hit.pos.x,
				hit.pos.y,
				hit.pos.z,
				ToStringRecoverReason(debugState.recoverReason),
				debugState.recoverSucceeded,
				debugState.recoverOverlapCount
			);
			mCollisionDebugLogCooldownSec = std::max(0.0f, logInterval);
		}
	}

	void GameMovementComponent::OnEditorTick(float) {
		auto*                     world     = GetWorld();
		const TransformComponent* transform = GetTransform();
		if (!world || !transform) {
			return;
		}

		world->GetDebugDraw().DrawBox(
			transform->GetPosition(),
			transform->GetRotation(),
			mBoxHalfExtents * 2.0f,
			Vec4::cyan
		);
	}

	std::string_view GameMovementComponent::GetStableName() const {
		return "game.GameMovement";
	}

	std::string_view GameMovementComponent::GetComponentName() const {
		return "GameMovement";
	}

	void GameMovementComponent::TeleportAndResetMotion(
		TransformComponent* transform,
		const Vec3&         worldPosition
	) {
		if (!transform) {
			return;
		}

		// リスポーン直後の慣性や支持床キャッシュを引きずらないように初期化します。
		transform->SetPosition(worldPosition);
		transform->RequestInterpolationResync();
		mVelocity                 = Vec3::zero;
		mGrounded                 = false;
		mSupportCache             = {};
		mJumpSnapDisableRemaining = 0.0f;
		UpdateCollisionHull(transform);
	}

	bool GameMovementComponent::UseSprintSpeedAsDefaultGroundSpeed() const {
		return false;
	}

	void GameMovementComponent::RegisterMovementModes(
		GameMovementStateMachine& stateMachine
	) {
		stateMachine.AddMode(std::make_shared<NoclipMovementMode>());
		stateMachine.AddMode(std::make_shared<AirMovementMode>());
		stateMachine.AddMode(std::make_shared<GroundMovementMode>());
	}

	void GameMovementComponent::RegisterMovementAbilities(
		GameMovementStateMachine& stateMachine
	) {
		stateMachine.AddAbility(std::make_shared<JumpMovementAbility>());
		stateMachine.AddAbility(std::make_shared<CrouchMovementAbility>());
	}

	MOVEMENT_MODE_ID GameMovementComponent::GetInitialMode() const {
		return MOVEMENT_MODE_ID::AIR;
	}

	MOVEMENT_MODE_ID GameMovementComponent::GetAirModeForTransitions() const {
		return MOVEMENT_MODE_ID::AIR;
	}

	std::string GameMovementComponent::ResolvePresentationStateName() const {
		return std::string(ToString(mCurrentModeId));
	}

	void GameMovementComponent::UpdateCollisionHull(
		TransformComponent* transform
	) const {
		if (!transform) {
			return;
		}

		auto* boxResolver = dynamic_cast<BoxKinematicCollisionResolver*>(
			mCollisionResolver.get()
		);
		if (boxResolver) {
			boxResolver->UpdateHull(transform->GetPosition(), mBoxHalfExtents);
		}
	}

	MovementCapabilitySet& GameMovementComponent::GetCapabilitySet() {
		return mCapabilitySet;
	}

	const MovementCapabilitySet& GameMovementComponent::GetCapabilitySet()
	const {
		return mCapabilitySet;
	}

	void GameMovementComponent::OnAfterCoreCueDispatch(
		const std::string_view,
		const std::string_view,
		const MovementFrameInput&,
		const bool,
		const bool,
		const float
	) {}

#ifdef _DEBUG
	void GameMovementComponent::DrawInspectorImGui() {
		BaseCharacterComponent::DrawInspectorImGui();

		ImGui::SeparatorText("Movement Capabilities");
		ImGui::Checkbox("Jump", &mCapabilitySet.jump);
		ImGui::Checkbox("Crouch", &mCapabilitySet.crouch);
		ImGui::Checkbox("Slide", &mCapabilitySet.slide);
		ImGui::Checkbox("WallRun", &mCapabilitySet.wallRun);
		ImGui::Checkbox("DoubleJump", &mCapabilitySet.doubleJump);
		ImGui::Checkbox("SpeedVault", &mCapabilitySet.speedVault);
		ImGui::Checkbox("Blink", &mCapabilitySet.blink);
		ImGui::Checkbox("Grapple", &mCapabilitySet.grapple);
		ImGui::Text(
			"Mode: %s | ActiveAbilities: 0x%016llX",
			ToString(mCurrentModeId).data(),
			static_cast<unsigned long long>(mActiveAbilityMask)
		);

		ImGui::SeparatorText("Gameplay Cue Debug");
		ImGui::Text(
			"Published Cues: %llu",
			static_cast<unsigned long long>(mDebugPublishedCueCount)
		);
		ImGui::Text("Last Cue ID: %s", mDebugLastPublishedCueId.c_str());
		ImGui::Text(
			"Last Cue Payload: %.3f / %.3f",
			mDebugLastPublishedCueValue,
			mDebugLastPublishedCueValue2
		);

		auto* boxResolver = dynamic_cast<BoxKinematicCollisionResolver*>(
			mCollisionResolver.get()
		);
		if (boxResolver) {
			const BoxKinematicCollisionResolver::CollisionDebugState& debugState
				=
				boxResolver->GetCollisionDebugState();

			ImGui::SeparatorText("Collision Debug");
			ImGui::Text(
				"BlockingHit: %s (count=%u)",
				debugState.hasBlockingHit ? "true" : "false",
				debugState.blockingHitCount
			);
			ImGui::Text(
				"Hit guid=%llu tri=%u toi=%.4f depth=%.5f",
				static_cast<unsigned long long>(debugState.blockingHit.
					hitEntityGuid),
				debugState.blockingHit.triIndex,
				debugState.blockingHit.toi,
				debugState.blockingHit.depth
			);
			ImGui::Text(
				"Hit flags: startSolid=%s allsolid=%s",
				debugState.blockingHit.startSolid ? "true" : "false",
				debugState.blockingHit.allsolid ? "true" : "false"
			);
			ImGui::Text(
				"Recover: attempted=%s succeeded=%s reason=%s overlaps=%d",
				debugState.recoverAttempted ? "true" : "false",
				debugState.recoverSucceeded ? "true" : "false",
				ToStringRecoverReason(debugState.recoverReason),
				debugState.recoverOverlapCount
			);
		}
	}
#endif

	void GameMovementComponent::Deserialize(const JsonReader& reader) {
		BaseCharacterComponent::Deserialize(reader);
		if (const JsonReader caps = reader["movementCapabilities"]; caps.
			Valid()) {
			mCapabilitySet.jump   = caps["jump"].GetBool(mCapabilitySet.jump);
			mCapabilitySet.crouch = caps["crouch"].GetBool(
				mCapabilitySet.crouch
			);
			mCapabilitySet.slide = caps["slide"].GetBool(mCapabilitySet.slide);
			mCapabilitySet.wallRun = caps["wallRun"].GetBool(
				mCapabilitySet.wallRun
			);
			mCapabilitySet.doubleJump = caps["doubleJump"].GetBool(
				mCapabilitySet.doubleJump
			);
			mCapabilitySet.speedVault = caps["speedVault"].GetBool(
				mCapabilitySet.speedVault
			);
			mCapabilitySet.blink = caps["blink"].GetBool(mCapabilitySet.blink);
			mCapabilitySet.grapple = caps["grapple"].GetBool(
				mCapabilitySet.grapple
			);
		}
	}

	void GameMovementComponent::Serialize(JsonWriter& writer) const {
		BaseCharacterComponent::Serialize(writer);
		writer.Key("movementCapabilities");
		writer.BeginObject();
		writer.Key("jump");
		writer.Write(mCapabilitySet.jump);
		writer.Key("crouch");
		writer.Write(mCapabilitySet.crouch);
		writer.Key("slide");
		writer.Write(mCapabilitySet.slide);
		writer.Key("wallRun");
		writer.Write(mCapabilitySet.wallRun);
		writer.Key("doubleJump");
		writer.Write(mCapabilitySet.doubleJump);
		writer.Key("speedVault");
		writer.Write(mCapabilitySet.speedVault);
		writer.Key("blink");
		writer.Write(mCapabilitySet.blink);
		writer.Key("grapple");
		writer.Write(mCapabilitySet.grapple);
		writer.EndObject();
	}

	Vec3 GameMovementComponent::SampleSupportContactVelocity(
		const uint64_t supportEntityGuid,
		const Vec3&    worldPoint
	) const {
		(void)supportEntityGuid;
		(void)worldPoint;
		return Vec3::zero;
	}

	void GameMovementComponent::WriteReplayState(
		nlohmann::json& outState
	) const {
		JsonWriter writer("");
		writer.BeginObject();

		// 速度の書き込み
		writer.Key("velocity");
		writer.BeginArray();
		writer.Write(mVelocity.x);
		writer.Write(mVelocity.y);
		writer.Write(mVelocity.z);
		writer.EndArray();

		writer.Key("grounded");
		writer.Write(mGrounded);

		writer.Key("collisionEnabled");
		writer.Write(mCollisionEnabled);

		writer.Key("jumpSnapDisableRemaining");
		writer.Write(mJumpSnapDisableRemaining);

		// サポート情報の書き込み
		writer.Key("support");
		writer.BeginObject();
		writer.Key("grounded");
		writer.Write(mSupportCache.grounded);
		writer.Key("supportEntityGuid");
		writer.Write(mSupportCache.supportEntityGuid);
		writer.Key("supportLinearVelocity");
		writer.BeginArray();
		writer.Write(mSupportCache.supportLinearVelocity.x);
		writer.Write(mSupportCache.supportLinearVelocity.y);
		writer.Write(mSupportCache.supportLinearVelocity.z);
		writer.EndArray();
		writer.Key("supportStepDelta");
		writer.BeginArray();
		writer.Write(mSupportCache.supportStepDelta.x);
		writer.Write(mSupportCache.supportStepDelta.y);
		writer.Write(mSupportCache.supportStepDelta.z);
		writer.EndArray();
		writer.EndObject();

		// ボックスの半経
		writer.Key("boxHalfExtents");
		writer.BeginArray();
		writer.Write(mBoxHalfExtents.x);
		writer.Write(mBoxHalfExtents.y);
		writer.Write(mBoxHalfExtents.z);
		writer.EndArray();

		writer.Key("movementRuntimeVersion");
		writer.Write(kMovementRuntimeVersion);

		writer.Key("modeId");
		writer.Write(static_cast<uint32_t>(mCurrentModeId));

		writer.Key("activeAbilityMask");
		writer.Write(mActiveAbilityMask);

		// 能力セットの書き込み
		writer.Key("capabilities");
		writer.BeginObject();
		writer.Key("jump");
		writer.Write(mCapabilitySet.jump);
		writer.Key("crouch");
		writer.Write(mCapabilitySet.crouch);
		writer.Key("slide");
		writer.Write(mCapabilitySet.slide);
		writer.Key("wallRun");
		writer.Write(mCapabilitySet.wallRun);
		writer.Key("doubleJump");
		writer.Write(mCapabilitySet.doubleJump);
		writer.Key("speedVault");
		writer.Write(mCapabilitySet.speedVault);
		writer.Key("blink");
		writer.Write(mCapabilitySet.blink);
		writer.Key("grapple");
		writer.Write(mCapabilitySet.grapple);
		writer.EndObject();

		// トランスフォームデータの書き込み
		if (TransformComponent* transform = GetTransform()) {
			const Vec3       position = transform->GetPosition();
			const Quaternion rotation = transform->GetRotation();

			writer.Key("position");
			writer.BeginArray();
			writer.Write(position.x);
			writer.Write(position.y);
			writer.Write(position.z);
			writer.EndArray();

			writer.Key("rotation");
			writer.BeginArray();
			writer.Write(rotation.x);
			writer.Write(rotation.y);
			writer.Write(rotation.z);
			writer.Write(rotation.w);
			writer.EndArray();
		}

		writer.EndObject();

		// JsonWriter から構築された JSON を outState に代入
		outState = writer.GetRoot();
	}

	void GameMovementComponent::ReadReplayState(const nlohmann::json& inState) {
		JsonReader reader(inState);
		if (!reader.Valid() || !reader.IsObject()) {
			return;
		}

		// 速度の読み込み
		mVelocity = reader["velocity"].GetVec3(mVelocity);

		// スカラー値の読み込み
		mGrounded         = reader["grounded"].GetBool(mGrounded);
		mCollisionEnabled = reader["collisionEnabled"].GetBool(mCollisionEnabled);
		mJumpSnapDisableRemaining =
			reader["jumpSnapDisableRemaining"].GetFloat(mJumpSnapDisableRemaining);

		// ボックスハーフエクステントの読み込み
		mBoxHalfExtents = reader["boxHalfExtents"].GetVec3(mBoxHalfExtents);

		// サポート情報の読み込み
		JsonReader supportReader = reader["support"];
		if (supportReader.Valid() && supportReader.IsObject()) {
			mSupportCache.grounded =
				supportReader["grounded"].GetBool(mSupportCache.grounded);
			mSupportCache.supportEntityGuid = static_cast<uint64_t>(
				supportReader["supportEntityGuid"].GetInt(
					static_cast<int>(mSupportCache.supportEntityGuid)
				)
			);
			mSupportCache.supportLinearVelocity =
				supportReader["supportLinearVelocity"].GetVec3(
					mSupportCache.supportLinearVelocity
				);
			mSupportCache.supportStepDelta =
				supportReader["supportStepDelta"].GetVec3(
					mSupportCache.supportStepDelta
				);
		}

		// トランスフォーム情報の読み込み
		if (TransformComponent* transform = GetTransform()) {
			JsonReader posReader = reader["position"];
			if (posReader.Valid() && posReader.IsArray()) {
				transform->SetPosition(posReader.GetVec3());
			}

			JsonReader rotReader = reader["rotation"];
			if (rotReader.Valid() && rotReader.IsArray() &&
				rotReader.Size() == 4) {
				transform->SetRotation(Quaternion(
					rotReader[0].GetFloat(),
					rotReader[1].GetFloat(),
					rotReader[2].GetFloat(),
					rotReader[3].GetFloat()
				));
			}
		}

		// ランタイムバージョンの確認
		const bool hasRuntimeVersion = reader["movementRuntimeVersion"].Valid();
		if (hasRuntimeVersion) {
			// modeId の読み込み
			const uint32_t rawMode =
				static_cast<uint32_t>(reader["modeId"].GetInt(
					static_cast<int>(mCurrentModeId)
				));
			if (rawMode < static_cast<uint32_t>(MOVEMENT_MODE_ID::COUNT)) {
				mCurrentModeId = static_cast<MOVEMENT_MODE_ID>(rawMode);
			}

			// アクティブアビリティマスクの読み込み
			mActiveAbilityMask = static_cast<uint32_t>(
				reader["activeAbilityMask"].GetInt(
					static_cast<int>(mActiveAbilityMask)
				)
			);

			// 能力セットの読み込み
			JsonReader capabilitiesReader = reader["capabilities"];
			if (capabilitiesReader.Valid() && capabilitiesReader.IsObject()) {
				mCapabilitySet.jump =
					capabilitiesReader["jump"].GetBool(mCapabilitySet.jump);
				mCapabilitySet.crouch =
					capabilitiesReader["crouch"].GetBool(mCapabilitySet.crouch);
				mCapabilitySet.slide =
					capabilitiesReader["slide"].GetBool(mCapabilitySet.slide);
				mCapabilitySet.wallRun =
					capabilitiesReader["wallRun"].GetBool(mCapabilitySet.wallRun);
				mCapabilitySet.doubleJump = capabilitiesReader["doubleJump"]
					.GetBool(mCapabilitySet.doubleJump);
				mCapabilitySet.speedVault = capabilitiesReader["speedVault"]
					.GetBool(mCapabilitySet.speedVault);
				mCapabilitySet.blink =
					capabilitiesReader["blink"].GetBool(mCapabilitySet.blink);
				mCapabilitySet.grapple =
					capabilitiesReader["grapple"].GetBool(mCapabilitySet.grapple);
			}
		} else if (reader["stateName"].Valid() &&
				   reader["stateName"].IsString()) {
			// レガシースキーマのサポート
			Warning(
				GetComponentName(),
				"Legacy movement replay schema detected. Deterministic compatibility is best-effort."
			);
			const std::string stateName =
				StrUtil::ToLowerCase(reader["stateName"].GetString());
			if (stateName.find("noclip") != std::string::npos) {
				mCurrentModeId = MOVEMENT_MODE_ID::NOCLIP;
			} else if (stateName.find("ground") != std::string::npos) {
				mCurrentModeId = MOVEMENT_MODE_ID::GROUND;
			} else {
				mCurrentModeId = MOVEMENT_MODE_ID::AIR;
			}
		}

		if (mStateMachine) {
			mStateMachine->SetInitialMode(mCurrentModeId);
		}
	}

	uint64_t GameMovementComponent::ComputeReplayStateHash() const {
		uint64_t hash = ReplayHash::Begin();
		ReplayHash::AppendFloating(hash, mVelocity.x);
		ReplayHash::AppendFloating(hash, mVelocity.y);
		ReplayHash::AppendFloating(hash, mVelocity.z);
		ReplayHash::AppendPod(hash, mGrounded);
		ReplayHash::AppendPod(hash, mCollisionEnabled);
		ReplayHash::AppendFloating(hash, mJumpSnapDisableRemaining);

		ReplayHash::AppendPod(hash, mSupportCache.grounded);
		ReplayHash::AppendPod(hash, mSupportCache.supportEntityGuid);
		ReplayHash::AppendFloating(hash, mSupportCache.supportLinearVelocity.x);
		ReplayHash::AppendFloating(hash, mSupportCache.supportLinearVelocity.y);
		ReplayHash::AppendFloating(hash, mSupportCache.supportLinearVelocity.z);
		ReplayHash::AppendFloating(hash, mSupportCache.supportStepDelta.x);
		ReplayHash::AppendFloating(hash, mSupportCache.supportStepDelta.y);
		ReplayHash::AppendFloating(hash, mSupportCache.supportStepDelta.z);
		ReplayHash::AppendFloating(hash, mBoxHalfExtents.x);
		ReplayHash::AppendFloating(hash, mBoxHalfExtents.y);
		ReplayHash::AppendFloating(hash, mBoxHalfExtents.z);

		ReplayHash::AppendPod(
			hash,
			static_cast<uint32_t>(kMovementRuntimeVersion)
		);
		ReplayHash::AppendPod(hash, static_cast<uint32_t>(mCurrentModeId));
		ReplayHash::AppendPod(hash, mActiveAbilityMask);
		ReplayHash::AppendPod(hash, mCapabilitySet.jump);
		ReplayHash::AppendPod(hash, mCapabilitySet.crouch);
		ReplayHash::AppendPod(hash, mCapabilitySet.slide);
		ReplayHash::AppendPod(hash, mCapabilitySet.wallRun);
		ReplayHash::AppendPod(hash, mCapabilitySet.doubleJump);
		ReplayHash::AppendPod(hash, mCapabilitySet.speedVault);
		ReplayHash::AppendPod(hash, mCapabilitySet.blink);
		ReplayHash::AppendPod(hash, mCapabilitySet.grapple);

		if (const TransformComponent* transform = GetTransform()) {
			const Vec3       position = transform->GetPosition();
			const Quaternion rotation = transform->GetRotation();
			ReplayHash::AppendFloating(hash, position.x);
			ReplayHash::AppendFloating(hash, position.y);
			ReplayHash::AppendFloating(hash, position.z);
			ReplayHash::AppendFloating(hash, rotation.x);
			ReplayHash::AppendFloating(hash, rotation.y);
			ReplayHash::AppendFloating(hash, rotation.z);
			ReplayHash::AppendFloating(hash, rotation.w);
		}

		return hash;
	}

	TransformComponent* GameMovementComponent::GetTransform() const {
		Entity* owner = GetOwner();
		return owner ? owner->GetComponent<TransformComponent>() : nullptr;
	}

	Vec3 GameMovementComponent::GetSupportSamplePoint(
		const TransformComponent* transform
	) const {
		if (!transform) {
			return Vec3::zero;
		}

		return transform->GetPosition() + Vec3::down * mBoxHalfExtents.y;
	}

	Vec3 GameMovementComponent::ResolveSupportLinearVelocity(
		const uint64_t supportEntityGuid
	) const {
		(void)supportEntityGuid;
		return Vec3::zero;
	}

	Vec3 GameMovementComponent::ResolveSupportAngularVelocity(
		const uint64_t supportEntityGuid
	) const {
		(void)supportEntityGuid;
		return Vec3::zero;
	}

	Vec3 GameMovementComponent::ResolveSupportContactVelocity(
		const uint64_t supportEntityGuid,
		const Vec3&    worldPoint
	) const {
		(void)supportEntityGuid;
		(void)worldPoint;
		return Vec3::zero;
	}

	Vec3 GameMovementComponent::ResolveSupportStepDelta(
		const uint64_t supportEntityGuid, const float stepSeconds
	) const {
		(void)supportEntityGuid;
		(void)stepSeconds;
		return Vec3::zero;
	}

	bool GameMovementComponent::ApplyPassiveMotionStep(
		TransformComponent* transform,
		const float         stepSeconds
	) {
		(void)transform;
		(void)stepSeconds;
		return false;
	}

	void GameMovementComponent::PublishCue(
		std::string id, const float value, const float value2
	) {
		World* world = GetWorld();
		if (!world) {
			return;
		}

		const Entity* owner = GetOwner();
		if (!owner) {
			return;
		}

		GameplayCue cue      = {};
		cue.id               = std::move(id);
		cue.sourceEntityGuid = owner->GetGuid();
		cue.value            = value;
		cue.value2           = value2;
		// value/value2 互換を維持しつつ、named payload も同時に供給します。
		if (cue.id == "movement.land") {
			cue.SetFloat("landStrength", value);
			cue.SetFloat("impactSpeedHuPerSec", value2);
		} else if (cue.id == "movement.footstep") {
			cue.SetFloat("footstepScale", value);
			cue.SetFloat("horizontalSpeedHuPerSec", value2);
		} else if (cue.id == "movement.sprint.update") {
			cue.SetFloat("sprintRatio", value);
		}
		world->GetGameplayCueBus().Publish(cue);

#ifdef _DEBUG
		mDebugLastPublishedCueId     = cue.id;
		mDebugLastPublishedCueValue  = cue.value;
		mDebugLastPublishedCueValue2 = cue.value2;
		++mDebugPublishedCueCount;
#endif
	}

	REGISTER_COMPONENT(GameMovementComponent);
}
