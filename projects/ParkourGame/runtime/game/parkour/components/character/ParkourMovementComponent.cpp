#include "ParkourMovementComponent.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Quaternion.h"
#include "core/math/Vec4.h"

// ReSharper disable once CppUnusedIncludeDirective
#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"
#include "engine/physics/core/Physics.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/primitive/Primitives.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/world/World.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"
#include "game/core/components/CameraRotatorComponent.h"
#include "game/core/components/character/state/GameMovementStateMachine.h"
#include "game/core/replay/ReplayHash.h"

#include "ability/ParkourMovementAbilities.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kStateParkourGroundMoveLower =
			"parkourgroundmove";
		constexpr float kForwardSprintDotThreshold = 0.7f;
		constexpr float kMinSprintMoveSpeedHu      = 5.0f;
		constexpr float kDuckSweepMinDistanceHu    = 0.05f;
		constexpr float kDuckSweepBlockToiEpsilon  = 1.0e-4f;
		constexpr float kUnduckHeadroomPaddingHu   = 0.5f;
		constexpr float kDuckViewUnduckHoldSec     = 0.05f;

		float EaseInOutSpline(float value) {
			value = std::clamp(value, 0.0f, 1.0f);
			const float valueSquared = value * value;
			return 3.0f * valueSquared - 2.0f * valueSquared * value;
		}

		bool TryReadVec3FromObject(
			const nlohmann::json& object,
			const char*           key,
			Vec3&                 outValue
		) {
			if (!object.is_object()) {
				return false;
			}
			const auto it = object.find(key);
			if (it == object.end() || !it->is_array() || it->size() != 3) {
				return false;
			}
			outValue = Vec3(
				(*it)[0].get<float>(),
				(*it)[1].get<float>(),
				(*it)[2].get<float>()
			);
			return true;
		}
	}

	ParkourMovementComponent::~ParkourMovementComponent() = default;

	void ParkourMovementComponent::OnAttached() {
		GameMovementComponent::OnAttached();
		mAutoSprintActive = false;
		ResetParkourRuntime();
		ResetDuckViewRuntime();
		mStandingHalfExtents = mBoxHalfExtents;
		RebuildDuckHalfExtents();
		mCameraRootBaseLocalCached = false;
		ClearDeterministicActionInputQueue();
		mActionFrameInput = {};
		auto& [jump, crouch, slide, wallRun, doubleJump, speedVault, blink,
			grapple] = GetCapabilitySet();
		jump         = true;
		crouch       = true;
		slide        = true;
		wallRun      = true;
		doubleJump   = true;
		speedVault   = true;
		blink        = true;
		grapple      = true;
	}

	void ParkourMovementComponent::SimulateStep(
		TransformComponent*       transform,
		const MovementFrameInput& input,
		const float               stepSeconds
	) {
		DeterministicActionInputPacket actionPacket = {};
		if (mDeterministicActionInputQueue.Pop(actionPacket)) {
			mActionFrameInput = actionPacket.input;
		} else {
			mActionFrameInput = {};
		}

		TickParkourTimers(stepSeconds);
		ResetDuckStandDebugFrame();
		GameMovementComponent::SimulateStep(transform, input, stepSeconds);
		UpdateDuckViewHeight(transform, input, stepSeconds);
		DrawDuckStandDebug(transform);
		// 旧State実装と同じ入力エッジ判定に合わせるため、毎tick最後に同期します。
		mRuntime.lastJumpHeld = input.jumpPressed;
		if (mGrounded) {
			mRuntime.hasDoubleJump = true;
		}
	}

	void ParkourMovementComponent::EnqueueDeterministicActionInput(
		const uint64_t                   tick,
		const float                      stepSeconds,
		const CharacterActionFrameInput& input
	) {
		mDeterministicActionInputQueue.Push(
			DeterministicActionInputPacket{
				.tick        = tick,
				.stepSeconds = stepSeconds,
				.input       = input
			}
		);
	}

	void ParkourMovementComponent::ClearDeterministicActionInputQueue() {
		mDeterministicActionInputQueue.Clear();
	}

	const CharacterActionFrameInput&
	ParkourMovementComponent::GetActionFrameInput() const {
		return mActionFrameInput;
	}

	std::string_view ParkourMovementComponent::GetStableName() const {
		return "parkour.ParkourMovement";
	}

	std::string_view ParkourMovementComponent::GetComponentName() const {
		return "ParkourMovement";
	}

#ifdef _DEBUG
	void ParkourMovementComponent::DrawInspectorImGui() {
		GameMovementComponent::DrawInspectorImGui();

		ImGui::SeparatorText("Parkour Runtime");
		ImGui::Text(
			"DoubleJump: %s", mRuntime.hasDoubleJump ? "true" : "false"
		);
		ImGui::Text("DuckHull: %s", mRuntime.duckHullActive ? "true" : "false");
		ImGui::Text(
			"WallRun: %s (%.2f s)", mRuntime.wallRun.active ? "true" : "false",
			mRuntime.wallRun.time
		);
		ImGui::Text(
			"Slide: %s (%.2f s)", mRuntime.slide.active ? "true" : "false",
			mRuntime.slide.time
		);
		ImGui::Text(
			"Blink: %s (cool %.2f s)", mRuntime.blink.active ? "true" : "false",
			mRuntime.blink.cooldown
		);
		ImGui::Text(
			"Vault: %s (cool %.2f s)", mRuntime.vault.active ? "true" : "false",
			mRuntime.vault.cooldown
		);
		ImGui::SeparatorText("Duck View");
		ImGui::Text("DuckViewFraction: %.2f", mDuckViewFraction);
		ImGui::Text(
			"DuckViewUnduckHoldRemaining: %.3f",
			mDuckViewUnduckHoldRemainingSec
		);
		ImGui::Text("DuckViewSmoothedLocalY: %.4f", mDuckViewSmoothedLocalY);
		ImGui::Text("CameraRootGuid: %llu", mCameraRootEntityGuid);
	}
#endif

	void ParkourMovementComponent::Deserialize(const JsonReader& reader) {
		GameMovementComponent::Deserialize(reader);
		if (reader.Has("cameraRootEntityGuid")) {
			mCameraRootEntityGuid = reader["cameraRootEntityGuid"].GetUint64();
		}
		mStandViewHeightHu = reader.Has("standViewHeightHu")
			                     ? reader["standViewHeightHu"].GetFloat() :
			                     mStandViewHeightHu;
		mDuckViewHeightHu = reader.Has("duckViewHeightHu")
			                    ? reader["duckViewHeightHu"].GetFloat() :
			                    mDuckViewHeightHu;
		mDuckViewTimeToDuckSec = reader.Has("duckViewTimeToDuckSec")
			                         ? reader["duckViewTimeToDuckSec"].GetFloat() :
			                         mDuckViewTimeToDuckSec;
		mDuckViewTimeToUnduckSec = reader.Has("duckViewTimeToUnduckSec")
			                           ? reader["duckViewTimeToUnduckSec"].
			                             GetFloat() :
			                           mDuckViewTimeToUnduckSec;
		mDuckViewClampPaddingHu = reader.Has("duckViewClampPaddingHu")
			                          ? reader["duckViewClampPaddingHu"].
			                            GetFloat() :
			                          mDuckViewClampPaddingHu;
		mStandViewHeightHu      = std::max(0.0f, mStandViewHeightHu);
		mDuckViewHeightHu = std::clamp(mDuckViewHeightHu, 0.0f, mStandViewHeightHu);
		mDuckViewTimeToDuckSec   = std::max(0.0f, mDuckViewTimeToDuckSec);
		mDuckViewTimeToUnduckSec = std::max(0.0f, mDuckViewTimeToUnduckSec);
		mDuckViewClampPaddingHu  = std::max(0.0f, mDuckViewClampPaddingHu);
		mStandingHalfExtents = mBoxHalfExtents;
		RebuildDuckHalfExtents();
		mCameraRootBaseLocalCached = false;
		ResetDuckViewRuntime();
	}

	void ParkourMovementComponent::Serialize(JsonWriter& writer) const {
		GameMovementComponent::Serialize(writer);
		writer.Key("cameraRootEntityGuid");
		writer.Write(mCameraRootEntityGuid);
		writer.Key("standViewHeightHu");
		writer.Write(mStandViewHeightHu);
		writer.Key("duckViewHeightHu");
		writer.Write(mDuckViewHeightHu);
		writer.Key("duckViewTimeToDuckSec");
		writer.Write(mDuckViewTimeToDuckSec);
		writer.Key("duckViewTimeToUnduckSec");
		writer.Write(mDuckViewTimeToUnduckSec);
		writer.Key("duckViewClampPaddingHu");
		writer.Write(mDuckViewClampPaddingHu);
	}

	void ParkourMovementComponent::WriteReplayState(
		nlohmann::json& outState
	) const {
		nlohmann::json baseState = nlohmann::json::object();
		GameMovementComponent::WriteReplayState(baseState);
		outState["base"] = std::move(baseState);

		outState["standingHalfExtents"] = nlohmann::json::array(
			{
				mStandingHalfExtents.x, mStandingHalfExtents.y,
				mStandingHalfExtents.z
			}
		);
		outState["duckHalfExtents"] = nlohmann::json::array(
			{mDuckHalfExtents.x, mDuckHalfExtents.y, mDuckHalfExtents.z}
		);
		outState["duckViewFraction"] = mDuckViewFraction;
		outState["duckViewUnduckHoldRemainingSec"] =
			mDuckViewUnduckHoldRemainingSec;
		outState["duckViewSmoothedLocalY"] = mDuckViewSmoothedLocalY;

		nlohmann::json runtime    = nlohmann::json::object();
		runtime["hasDoubleJump"]  = mRuntime.hasDoubleJump;
		runtime["lastJumpHeld"]   = mRuntime.lastJumpHeld;
		runtime["duckHullActive"] = mRuntime.duckHullActive;
		runtime["wallRun"]        = nlohmann::json::object(
			{
				{"active", mRuntime.wallRun.active},
				{
					"normal", nlohmann::json::array(
						{
							mRuntime.wallRun.normal.x,
							mRuntime.wallRun.normal.y, mRuntime.wallRun.normal.z
						}
					)
				},
				{
					"direction", nlohmann::json::array(
						{
							mRuntime.wallRun.direction.x,
							mRuntime.wallRun.direction.y,
							mRuntime.wallRun.direction.z
						}
					)
				},
				{"time", mRuntime.wallRun.time},
				{"timeSinceLast", mRuntime.wallRun.timeSinceLast},
				{
					"lastWallNormal", nlohmann::json::array(
						{
							mRuntime.wallRun.lastWallNormal.x,
							mRuntime.wallRun.lastWallNormal.y,
							mRuntime.wallRun.lastWallNormal.z
						}
					)
				},
				{"jumpWasHeldOnInit", mRuntime.wallRun.jumpWasHeldOnInit}
			}
		);
		runtime["slide"] = nlohmann::json::object(
			{
				{"active", mRuntime.slide.active},
				{
					"direction", nlohmann::json::array(
						{
							mRuntime.slide.direction.x,
							mRuntime.slide.direction.y,
							mRuntime.slide.direction.z
						}
					)
				},
				{"time", mRuntime.slide.time}
			}
		);
		runtime["blink"] = nlohmann::json::object(
			{
				{"cooldown", mRuntime.blink.cooldown},
				{"active", mRuntime.blink.active},
				{"moveTime", mRuntime.blink.moveTime},
				{
					"startPos", nlohmann::json::array(
						{
							mRuntime.blink.startPos.x,
							mRuntime.blink.startPos.y,
							mRuntime.blink.startPos.z
						}
					)
				},
				{
					"targetPos", nlohmann::json::array(
						{
							mRuntime.blink.targetPos.x,
							mRuntime.blink.targetPos.y,
							mRuntime.blink.targetPos.z
						}
					)
				}
			}
		);
		runtime["vault"] = nlohmann::json::object(
			{
				{"active", mRuntime.vault.active},
				{"time", mRuntime.vault.time},
				{"cooldown", mRuntime.vault.cooldown},
				{
					"startPos", nlohmann::json::array(
						{
							mRuntime.vault.startPos.x,
							mRuntime.vault.startPos.y,
							mRuntime.vault.startPos.z
						}
					)
				},
				{
					"apexPos", nlohmann::json::array(
						{
							mRuntime.vault.apexPos.x,
							mRuntime.vault.apexPos.y,
							mRuntime.vault.apexPos.z
						}
					)
				},
				{
					"endPos", nlohmann::json::array(
						{
							mRuntime.vault.endPos.x,
							mRuntime.vault.endPos.y,
							mRuntime.vault.endPos.z
						}
					)
				},
				{
					"preVelocity", nlohmann::json::array(
						{
							mRuntime.vault.preVelocity.x,
							mRuntime.vault.preVelocity.y,
							mRuntime.vault.preVelocity.z
						}
					)
				}
			}
		);
		runtime["grapple"] = nlohmann::json::object(
			{
				{"active", mRuntime.grapple.isActive},
				{"latched", mRuntime.grapple.isLatched},
				{
					"anchorPoint", nlohmann::json::array(
						{
							mRuntime.grapple.anchorPoint.x,
							mRuntime.grapple.anchorPoint.y,
							mRuntime.grapple.anchorPoint.z
						}
					)
				},
				{"ropeLength", mRuntime.grapple.ropeLength},
				{"minRopeLength", mRuntime.grapple.minRopeLength},
				{"maxRopeLength", mRuntime.grapple.maxRopeLength}
			}
		);

		outState["runtime"] = std::move(runtime);
	}

	void ParkourMovementComponent::ReadReplayState(
		const nlohmann::json& inState
	) {
		if (!inState.is_object()) {
			return;
		}

		if (const auto itBase = inState.find("base");
			itBase != inState.end() && itBase->is_object()) {
			GameMovementComponent::ReadReplayState(*itBase);
		} else {
			// backward compatibility for older/flat dump styles
			GameMovementComponent::ReadReplayState(inState);
		}
		(void)TryReadVec3FromObject(
			inState,
			"standingHalfExtents",
			mStandingHalfExtents
		);
		(void)TryReadVec3FromObject(
			inState,
			"duckHalfExtents",
			mDuckHalfExtents
		);

		if (const auto itRuntime = inState.find("runtime");
			itRuntime != inState.end() && itRuntime->is_object()) {
			const nlohmann::json& runtime = *itRuntime;
			mRuntime.hasDoubleJump        = runtime.value(
				"hasDoubleJump",
				mRuntime.hasDoubleJump
			);
			mRuntime.lastJumpHeld = runtime.value(
				"lastJumpHeld",
				mRuntime.lastJumpHeld
			);
			mRuntime.duckHullActive = runtime.value(
				"duckHullActive",
				mRuntime.duckHullActive
			);
			if (const auto itWallRun = runtime.find("wallRun");
				itWallRun != runtime.end() && itWallRun->is_object()) {
				const nlohmann::json& wallRun = *itWallRun;
				mRuntime.wallRun.active       = wallRun.value(
					"active",
					mRuntime.wallRun.active
				);
				(void)TryReadVec3FromObject(
					wallRun,
					"normal",
					mRuntime.wallRun.normal
				);
				(void)TryReadVec3FromObject(
					wallRun,
					"direction",
					mRuntime.wallRun.direction
				);
				mRuntime.wallRun.time = wallRun.value(
					"time",
					mRuntime.wallRun.time
				);
				mRuntime.wallRun.timeSinceLast = wallRun.value(
					"timeSinceLast",
					mRuntime.wallRun.timeSinceLast
				);
				(void)TryReadVec3FromObject(
					wallRun,
					"lastWallNormal",
					mRuntime.wallRun.lastWallNormal
				);
				mRuntime.wallRun.jumpWasHeldOnInit = wallRun.value(
					"jumpWasHeldOnInit",
					mRuntime.wallRun.jumpWasHeldOnInit
				);
			}

			if (const auto itSlide = runtime.find("slide");
				itSlide != runtime.end() && itSlide->is_object()) {
				const nlohmann::json& slide = *itSlide;
				mRuntime.slide.active       = slide.value(
					"active",
					mRuntime.slide.active
				);
				(void)TryReadVec3FromObject(
					slide,
					"direction",
					mRuntime.slide.direction
				);
				mRuntime.slide.time = slide.value(
					"time",
					mRuntime.slide.time
				);
			}

			if (const auto itBlink = runtime.find("blink");
				itBlink != runtime.end() && itBlink->is_object()) {
				const nlohmann::json& blink = *itBlink;
				mRuntime.blink.cooldown     = blink.value(
					"cooldown",
					mRuntime.blink.cooldown
				);
				mRuntime.blink.active = blink.value(
					"active",
					mRuntime.blink.active
				);
				mRuntime.blink.moveTime = blink.value(
					"moveTime",
					mRuntime.blink.moveTime
				);
				(void)TryReadVec3FromObject(
					blink,
					"startPos",
					mRuntime.blink.startPos
				);
				(void)TryReadVec3FromObject(
					blink,
					"targetPos",
					mRuntime.blink.targetPos
				);
			}

			if (const auto itVault = runtime.find("vault");
				itVault != runtime.end() && itVault->is_object()) {
				const nlohmann::json& vault = *itVault;
				mRuntime.vault.active       = vault.value(
					"active",
					mRuntime.vault.active
				);
				mRuntime.vault.time = vault.value(
					"time",
					mRuntime.vault.time
				);
				mRuntime.vault.cooldown = vault.value(
					"cooldown",
					mRuntime.vault.cooldown
				);
				(void)TryReadVec3FromObject(
					vault,
					"startPos",
					mRuntime.vault.startPos
				);
				(void)TryReadVec3FromObject(
					vault,
					"apexPos",
					mRuntime.vault.apexPos
				);
				(void)TryReadVec3FromObject(
					vault,
					"endPos",
					mRuntime.vault.endPos
				);
				(void)TryReadVec3FromObject(
					vault,
					"preVelocity",
					mRuntime.vault.preVelocity
				);
			}

			if (const auto itGrapple = runtime.find("grapple");
				itGrapple != runtime.end() && itGrapple->is_object()) {
				const nlohmann::json& grapple = *itGrapple;
				mRuntime.grapple.isActive     = grapple.value(
					"active",
					mRuntime.grapple.isActive
				);
				mRuntime.grapple.isLatched = grapple.value(
					"latched",
					mRuntime.grapple.isLatched
				);
				(void)TryReadVec3FromObject(
					grapple,
					"anchorPoint",
					mRuntime.grapple.anchorPoint
				);
				mRuntime.grapple.ropeLength = grapple.value(
					"ropeLength",
					mRuntime.grapple.ropeLength
				);
				mRuntime.grapple.minRopeLength = grapple.value(
					"minRopeLength",
					mRuntime.grapple.minRopeLength
				);
				mRuntime.grapple.maxRopeLength = grapple.value(
					"maxRopeLength",
					mRuntime.grapple.maxRopeLength
				);
			}
		}
		mDuckViewFraction = std::clamp(
			inState.value("duckViewFraction", mRuntime.duckHullActive ? 1.0f : 0.0f),
			0.0f,
			1.0f
		);
		mDuckViewUnduckHoldRemainingSec = std::max(
			0.0f,
			inState.value("duckViewUnduckHoldRemainingSec", 0.0f)
		);
		mDuckViewSmoothedLocalY = inState.value(
			"duckViewSmoothedLocalY",
			mDuckViewSmoothedLocalY
		);
		mDuckViewFractionInitialized = true;
		mDuckViewSmoothedLocalYInitialized = true;

		if (mStandingHalfExtents.IsZero()) {
			mStandingHalfExtents = mBoxHalfExtents;
		}
		if (mDuckHalfExtents.IsZero()) {
			RebuildDuckHalfExtents();
		}
		mBoxHalfExtents = mRuntime.duckHullActive ?
			                  mDuckHalfExtents :
			                  mStandingHalfExtents;

		if (TransformComponent* transform = GetTransform()) {
			UpdateCollisionHull(transform);
		}
		mCameraRootBaseLocalCached = false;
	}

	uint64_t ParkourMovementComponent::ComputeReplayStateHash() const {
		uint64_t       hash     = ReplayHash::Begin();
		const uint64_t baseHash =
			GameMovementComponent::ComputeReplayStateHash();
		ReplayHash::AppendPod(hash, baseHash);
		ReplayHash::AppendFloating(hash, mStandingHalfExtents.x);
		ReplayHash::AppendFloating(hash, mStandingHalfExtents.y);
		ReplayHash::AppendFloating(hash, mStandingHalfExtents.z);
		ReplayHash::AppendFloating(hash, mDuckHalfExtents.x);
		ReplayHash::AppendFloating(hash, mDuckHalfExtents.y);
		ReplayHash::AppendFloating(hash, mDuckHalfExtents.z);
		ReplayHash::AppendFloating(hash, mDuckViewFraction);
		ReplayHash::AppendFloating(hash, mDuckViewUnduckHoldRemainingSec);
		ReplayHash::AppendFloating(hash, mDuckViewSmoothedLocalY);

		ReplayHash::AppendPod(hash, mRuntime.hasDoubleJump);
		ReplayHash::AppendPod(hash, mRuntime.lastJumpHeld);
		ReplayHash::AppendPod(hash, mRuntime.duckHullActive);

		ReplayHash::AppendPod(hash, mRuntime.wallRun.active);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.normal.x);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.normal.y);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.normal.z);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.direction.x);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.direction.y);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.direction.z);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.time);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.timeSinceLast);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.lastWallNormal.x);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.lastWallNormal.y);
		ReplayHash::AppendFloating(hash, mRuntime.wallRun.lastWallNormal.z);
		ReplayHash::AppendPod(hash, mRuntime.wallRun.jumpWasHeldOnInit);

		ReplayHash::AppendPod(hash, mRuntime.slide.active);
		ReplayHash::AppendFloating(hash, mRuntime.slide.direction.x);
		ReplayHash::AppendFloating(hash, mRuntime.slide.direction.y);
		ReplayHash::AppendFloating(hash, mRuntime.slide.direction.z);
		ReplayHash::AppendFloating(hash, mRuntime.slide.time);

		ReplayHash::AppendPod(hash, mRuntime.blink.active);
		ReplayHash::AppendFloating(hash, mRuntime.blink.cooldown);
		ReplayHash::AppendFloating(hash, mRuntime.blink.moveTime);
		ReplayHash::AppendFloating(hash, mRuntime.blink.startPos.x);
		ReplayHash::AppendFloating(hash, mRuntime.blink.startPos.y);
		ReplayHash::AppendFloating(hash, mRuntime.blink.startPos.z);
		ReplayHash::AppendFloating(hash, mRuntime.blink.targetPos.x);
		ReplayHash::AppendFloating(hash, mRuntime.blink.targetPos.y);
		ReplayHash::AppendFloating(hash, mRuntime.blink.targetPos.z);

		ReplayHash::AppendPod(hash, mRuntime.vault.active);
		ReplayHash::AppendFloating(hash, mRuntime.vault.time);
		ReplayHash::AppendFloating(hash, mRuntime.vault.cooldown);
		ReplayHash::AppendFloating(hash, mRuntime.vault.startPos.x);
		ReplayHash::AppendFloating(hash, mRuntime.vault.startPos.y);
		ReplayHash::AppendFloating(hash, mRuntime.vault.startPos.z);
		ReplayHash::AppendFloating(hash, mRuntime.vault.apexPos.x);
		ReplayHash::AppendFloating(hash, mRuntime.vault.apexPos.y);
		ReplayHash::AppendFloating(hash, mRuntime.vault.apexPos.z);
		ReplayHash::AppendFloating(hash, mRuntime.vault.endPos.x);
		ReplayHash::AppendFloating(hash, mRuntime.vault.endPos.y);
		ReplayHash::AppendFloating(hash, mRuntime.vault.endPos.z);
		ReplayHash::AppendFloating(hash, mRuntime.vault.preVelocity.x);
		ReplayHash::AppendFloating(hash, mRuntime.vault.preVelocity.y);
		ReplayHash::AppendFloating(hash, mRuntime.vault.preVelocity.z);
		ReplayHash::AppendPod(hash, mRuntime.grapple.isActive);
		ReplayHash::AppendPod(hash, mRuntime.grapple.isLatched);
		ReplayHash::AppendFloating(hash, mRuntime.grapple.anchorPoint.x);
		ReplayHash::AppendFloating(hash, mRuntime.grapple.anchorPoint.y);
		ReplayHash::AppendFloating(hash, mRuntime.grapple.anchorPoint.z);
		ReplayHash::AppendFloating(hash, mRuntime.grapple.ropeLength);
		ReplayHash::AppendFloating(hash, mRuntime.grapple.minRopeLength);
		ReplayHash::AppendFloating(hash, mRuntime.grapple.maxRopeLength);
		return hash;
	}

	uint32_t ParkourMovementComponent::GetIcon() const {
		return kIconDirectionsRun;
	}

	ParkourMovementComponent::ParkourRuntime&
	ParkourMovementComponent::GetParkourRuntime() {
		return mRuntime;
	}

	const ParkourMovementComponent::ParkourRuntime&
	ParkourMovementComponent::GetParkourRuntime() const {
		return mRuntime;
	}

	void ParkourMovementComponent::TickParkourTimers(const float deltaTime) {
		const float clampedDt          = std::max(0.0f, deltaTime);
		mRuntime.wallRun.timeSinceLast += clampedDt;
		mRuntime.blink.cooldown        = std::max(
			0.0f, mRuntime.blink.cooldown - clampedDt
		);
		mRuntime.vault.cooldown = std::max(
			0.0f, mRuntime.vault.cooldown - clampedDt
		);
	}

	void ParkourMovementComponent::PublishParkourMovementCue(
		const std::string_view id,
		const float            value,
		const float            value2
	) {
		PublishCue(std::string(id), value, value2);
	}

	bool ParkourMovementComponent::SetDuckHullEnabled(
		MovementContext& context,
		const bool       enabled
	) {
		return enabled ? ApplyDuckHull(context) : ApplyStandHull(context);
	}

	bool ParkourMovementComponent::IsDuckHullEnabled() const {
		return mRuntime.duckHullActive;
	}

	bool ParkourMovementComponent::CanStandAt(
		const MovementContext& context
	) const {
		if (!context.transform) {
			return true;
		}

		const bool debugEnabled = IsDuckDebugDrawEnabled();
		if (debugEnabled) {
			mDuckStandDebug.evaluateStandCalled = true;
			mDuckStandDebug.standAllowed = false;
			mDuckStandDebug.grounded = IsDuckGrounded(context);
			mDuckStandDebug.currentCenter = context.transform->GetPosition();
			mDuckStandDebug.currentHalfExtents = mBoxHalfExtents;
		}

		Vec3       standCenter = context.transform->GetPosition();
		const bool grounded    = IsDuckGrounded(context);
		if (mRuntime.duckHullActive) {
			standCenter += ComputeHullCenterOffsetForDuckState(context, false);
		}

		Vec3        standCheckCenter = standCenter;
		Vec3        standCheckHalfExtents = mStandingHalfExtents;
		const float headroomPadM = Math::HtoM(kUnduckHeadroomPaddingHu);
		if (headroomPadM > 0.0f) {
			// 立ち判定で天井側のみわずかにマージンを持たせ、境界接触での誤復帰を防ぎます。
			standCheckCenter        += Vec3::up * (headroomPadM * 0.5f);
			standCheckHalfExtents.y += headroomPadM * 0.5f;
		}
		if (debugEnabled) {
			mDuckStandDebug.standTargetCenter      = standCheckCenter;
			mDuckStandDebug.standTargetHalfExtents = standCheckHalfExtents;
		}

		// 地上 unduck は足元固定で遷移するため、終点占有判定を優先します。
		// 空中 unduck のみ経路スイープを有効化して干渉を防ぎます。
		HullOccupancyDebugInfo occupancyDebug = {};
		if (!CanOccupyHull(
			context,
			standCheckCenter,
			standCheckHalfExtents,
			!grounded,
			debugEnabled ? &occupancyDebug : nullptr
		)) {
			if (debugEnabled) {
				mDuckStandDebug.standOccupancy = occupancyDebug;
			}
			return false;
		}
		if (debugEnabled) {
			mDuckStandDebug.standOccupancy = occupancyDebug;
		}

		if (!grounded || !mRuntime.duckHullActive) {
			if (debugEnabled) {
				mDuckStandDebug.standAllowed = true;
			}
			return true;
		}

		const auto* physics = context.resolver ?
			                      context.resolver->GetPhysics() :
			                      nullptr;
		if (!physics) {
			if (debugEnabled) {
				mDuckStandDebug.standAllowed = true;
			}
			return true;
		}

		// 地上 unduck は「立ちで追加される上部体積」を専用スラブとして上方向にスイープします。
		// duck ハル全体を上げるだけでは、上端側の追加体積を取りこぼすケースがあります。
		const float deltaHalfY = std::max(
			0.0f,
			mStandingHalfExtents.y - mDuckHalfExtents.y
		);
		const float castLength = deltaHalfY + std::max(0.0f, headroomPadM);
		if (castLength <= 1.0e-6f || deltaHalfY <= 1.0e-6f) {
			if (debugEnabled) {
				mDuckStandDebug.standAllowed = true;
			}
			return true;
		}

		Vec3 headProbeHalf     = mStandingHalfExtents;
		headProbeHalf.y        = deltaHalfY * 0.5f + headroomPadM * 0.5f;
		const Box headProbeBox = {
			.center = context.transform->GetPosition() +
			          Vec3::up * (mDuckHalfExtents.y + headProbeHalf.y),
			.halfSize = headProbeHalf
		};
		if (debugEnabled) {
			mDuckStandDebug.headSweepUsed            = true;
			mDuckStandDebug.headSweepStartCenter     = headProbeBox.center;
			mDuckStandDebug.headSweepHalfExtents     = headProbeBox.halfSize;
			mDuckStandDebug.headSweepLength          = castLength;
			mDuckStandDebug.headSweepReachableCenter =
				headProbeBox.center + Vec3::up * castLength;
		}
		Physics::Hit sweepHit{};
		if (!physics->BoxCast(headProbeBox, Vec3::up, castLength, &sweepHit)) {
			if (debugEnabled) {
				mDuckStandDebug.standAllowed = true;
			}
			return true;
		}
		const float hitToi  = std::clamp(sweepHit.toi, 0.0f, 1.0f);
		const bool  blocked = sweepHit.startSolid || sweepHit.allsolid ||
		                      sweepHit.toi < 1.0f - kDuckSweepBlockToiEpsilon;
		if (debugEnabled) {
			mDuckStandDebug.headSweepHitToi          = hitToi;
			mDuckStandDebug.headSweepBlocked         = blocked;
			mDuckStandDebug.headSweepHitPos          = sweepHit.pos;
			mDuckStandDebug.headSweepHitNormal       = sweepHit.normal;
			mDuckStandDebug.headSweepReachableCenter =
				headProbeBox.center + Vec3::up * (castLength * hitToi);
			mDuckStandDebug.standAllowed = !blocked;
		}
		return !blocked;
	}

	float ParkourMovementComponent::GetHorizontalSpeedHu(
		const Vec3& velocity
	) const {
		Vec3 horizontal = velocity;
		horizontal.y    = 0.0f;
		return Math::MtoH(horizontal.Length());
	}

	bool ParkourMovementComponent::ShouldSlideFromSpeed(
		const float horizontalSpeedHu
	) const {
		const float minSlideSpeedHu = mConsole ?
			                              mConsole->GetConVarValueOr(
				                              "park_slide_minspeed", 200.0f
			                              ) :
			                              200.0f;
		return horizontalSpeedHu >= minSlideSpeedHu;
	}

	bool ParkourMovementComponent::WantsDuckStance(
		const MovementContext& context
	) const {
		if (context.input.crouchPressed) {
			return true;
		}
		if (!CanStandAt(context)) {
			return true;
		}
		return (mActiveAbilityMask & AbilitySlotToMask(
			        MOVEMENT_ABILITY_SLOT::SLIDE
		        )
		       ) != 0;
	}

	void ParkourMovementComponent::SyncCollisionHull(
		TransformComponent* transform
	) {
		UpdateCollisionHull(transform);
	}

	void ParkourMovementComponent::EndWallRun() {
		if (!mRuntime.wallRun.active) {
			return;
		}

		mRuntime.wallRun.active            = false;
		mRuntime.wallRun.time              = 0.0f;
		mRuntime.wallRun.lastWallNormal    = mRuntime.wallRun.normal;
		mRuntime.wallRun.timeSinceLast     = 0.0f;
		mRuntime.wallRun.jumpWasHeldOnInit = false;
	}

	void ParkourMovementComponent::RegisterMovementModes(
		GameMovementStateMachine& stateMachine
	) {
		GameMovementComponent::RegisterMovementModes(stateMachine);
	}

	void ParkourMovementComponent::RegisterMovementAbilities(
		GameMovementStateMachine& stateMachine
	) {
		RegisterParkourMovementAbilities(stateMachine);
	}

	MOVEMENT_MODE_ID ParkourMovementComponent::GetInitialMode() const {
		return MOVEMENT_MODE_ID::AIR;
	}

	MOVEMENT_MODE_ID
	ParkourMovementComponent::GetAirModeForTransitions() const {
		return MOVEMENT_MODE_ID::AIR;
	}

	std::string ParkourMovementComponent::ResolvePresentationStateName() const {
		if (
			(mActiveAbilityMask & AbilitySlotToMask(
				 MOVEMENT_ABILITY_SLOT::BLINK
			 )) !=
			0
		) {
			return "ParkourBlinkMove";
		}
		if (
			(mActiveAbilityMask &
			 AbilitySlotToMask(MOVEMENT_ABILITY_SLOT::SPEED_VAULT)) != 0
		) {
			return "ParkourSpeedVaultMove";
		}
		if (
			(mActiveAbilityMask &
			 AbilitySlotToMask(MOVEMENT_ABILITY_SLOT::WALL_RUN)) != 0
		) {
			return "ParkourWallRunMove";
		}
		if (
			(mActiveAbilityMask & AbilitySlotToMask(
				 MOVEMENT_ABILITY_SLOT::SLIDE
			 )) !=
			0
		) {
			return "ParkourSlideMove";
		}

		if (mCurrentModeId == MOVEMENT_MODE_ID::GROUND) {
			return mRuntime.duckHullActive ?
				       std::string("ParkourDuckMove") :
				       std::string("ParkourGroundMove");
		}
		if (mCurrentModeId == MOVEMENT_MODE_ID::NOCLIP) {
			return "NoclipMove";
		}
		return "ParkourAirMove";
	}

	bool ParkourMovementComponent::UseSprintSpeedAsDefaultGroundSpeed() const {
		return true;
	}

	void ParkourMovementComponent::OnAfterCoreCueDispatch(
		const std::string_view,
		const std::string_view    currentStateName,
		const MovementFrameInput& input,
		const bool,
		const bool  isGrounded,
		const float stepSeconds
	) {
		const std::string loweredState = StrUtil::ToLowerCase(
			currentStateName.data()
		);
		const bool isParkourGroundState = loweredState ==
		                                  kStateParkourGroundMoveLower;
		const bool isWallRunState = loweredState == "parkourwallrunmove";
		const bool isSlideState   = loweredState == "parkourslidemove";

		const float sprintSpeedHu = mConsole ?
			                            mConsole->GetConVarValueOr(
				                            "sv_sprintspeed",
				                            320.0f
			                            ) :
			                            320.0f;
		const float sprintSpeedSafeHu = std::max(1.0f, sprintSpeedHu);

		Vec3 horizontalVelocity       = mVelocity;
		horizontalVelocity.y          = 0.0f;
		const float horizontalSpeedHu = Math::MtoH(horizontalVelocity.Length());

		Vec3 horizontalForward   = input.forward;
		horizontalForward.y      = 0.0f;
		const bool hasForwardDir = !horizontalForward.IsZero();
		if (hasForwardDir) {
			horizontalForward.Normalize();
		}
		const bool  hasHorizontalVelocity = !horizontalVelocity.IsZero();
		const float forwardDot = hasForwardDir && hasHorizontalVelocity ?
			                         horizontalForward.Dot(
				                         horizontalVelocity.Normalized()
			                         ) :
			                         -1.0f;

		const bool shouldSprint = isGrounded &&
		                          isParkourGroundState &&
		                          hasForwardDir &&
		                          hasHorizontalVelocity &&
		                          horizontalSpeedHu >= kMinSprintMoveSpeedHu &&
		                          forwardDot >= kForwardSprintDotThreshold;
		if (shouldSprint != mAutoSprintActive) {
			// movement.sprint.start / movement.sprint.end payload contract:
			// value  = 未使用(0)
			// value2 = 未使用(0)
			PublishCue(
				shouldSprint ? "movement.sprint.start" : "movement.sprint.end"
			);
			mAutoSprintActive = shouldSprint;
		}
		if (shouldSprint) {
			const float sprintRatio = std::clamp(
				horizontalSpeedHu / sprintSpeedSafeHu, 0.0f, 2.0f
			);
			// movement.sprint.update payload contract:
			// value  = スプリント比率 [0..2] (horizontalSpeedHu / sv_sprintspeed)
			// value2 = 未使用(0)
			// named:
			//   payload.sprintRatio
			PublishCue("movement.sprint.update", sprintRatio);
		}

		if (!loweredState.empty()) {
			// movement.state.update.* payload contract:
			// value  = 状態更新の強度(現在アクティブ時は1.0)
			// value2 = 未使用(0)
			PublishCue("movement.state.update." + loweredState, 1.0f);
		}

		if (mRuntime.pendingDoubleJumpCue) {
			// movement.doublejump: 現状は payload 未使用（value/value2 は 0）。
			PublishCue("movement.doublejump");
			mRuntime.pendingDoubleJumpCue = false;
		}

		const bool allowFootstep = isGrounded || isWallRunState;
		if (!allowFootstep || isSlideState) {
			mRuntime.footstepDistanceAccumHu = 0.0f;
			return;
		}

		const float footstepMinSpeedHu = mConsole ?
			                                 mConsole->GetConVarValueOr(
				                                 "park_footstep_minspeed", 80.0f
			                                 ) :
			                                 80.0f;
		if (horizontalSpeedHu < footstepMinSpeedHu || stepSeconds <= 0.0f) {
			mRuntime.footstepDistanceAccumHu = 0.0f;
			return;
		}

		const float footstepStrideHu = mConsole ?
			                               mConsole->GetConVarValueOr(
				                               "park_footstep_stride", 100.0f
			                               ) :
			                               170.0f;
		const float footstepMaxSpeedHu = mConsole ?
			                                 mConsole->GetConVarValueOr(
				                                 "park_footstep_maxspeed",
				                                 600.0f
			                                 ) :
			                                 360.0f;
		const float cappedSpeedHu = std::clamp(
			horizontalSpeedHu,
			0.0f,
			std::max(footstepMinSpeedHu, footstepMaxSpeedHu)
		);
		const float safeStrideHu         = std::max(1.0f, footstepStrideHu);
		mRuntime.footstepDistanceAccumHu += cappedSpeedHu * stepSeconds;
		const float footstepScale        = std::clamp(
			cappedSpeedHu / sprintSpeedSafeHu, 0.0f, 2.0f
		);
		while (mRuntime.footstepDistanceAccumHu >= safeStrideHu) {
			mRuntime.footstepDistanceAccumHu -= safeStrideHu;
			// movement.footstep payload contract:
			// value  = footstepScale (speed-normalized intensity, [0..2])
			// value2 = capped horizontal speed in HU/s
			// named:
			//   payload.footstepScale
			//   payload.horizontalSpeedHuPerSec
			PublishCue("movement.footstep", footstepScale, cappedSpeedHu);
		}
	}

	void ParkourMovementComponent::RebuildDuckHalfExtents() {
		const float duckScale = std::clamp(
			mConsole ?
				mConsole->GetConVarValueOr("park_duck_heightscale", 0.5f) :
				0.5f,
			0.2f,
			1.0f
		);
		mDuckHalfExtents   = mStandingHalfExtents;
		mDuckHalfExtents.y = mStandingHalfExtents.y * duckScale;
	}

	void ParkourMovementComponent::ResetParkourRuntime() {
		mRuntime                       = {};
		mRuntime.wallRun.timeSinceLast = 999.0f;
	}

	void ParkourMovementComponent::ResetDuckViewRuntime() {
		mDuckViewFraction            = mRuntime.duckHullActive ? 1.0f : 0.0f;
		mDuckViewUnduckHoldRemainingSec = 0.0f;
		mDuckViewSmoothedLocalY         = 0.0f;
		mDuckViewFractionInitialized = false;
		mDuckViewSmoothedLocalYInitialized = false;
	}

	TransformComponent* ParkourMovementComponent::ResolveCameraRootTransform(
		TransformComponent* actorTransform
	) {
		if (!actorTransform) {
			return nullptr;
		}
		Scene* scene = GetScene();
		if (!scene) {
			return nullptr;
		}

		const auto matchesOwner = [actorTransform](const TransformComponent* transform) {
			return transform && transform->GetParent() == actorTransform;
		};

		if (mCameraRootEntityGuid != 0) {
			if (Entity* entity = scene->FindEntity(mCameraRootEntityGuid)) {
				if (auto* transform = entity->GetComponent<TransformComponent>();
				    matchesOwner(transform)) {
					return transform;
				}
			}
		}

		for (const auto& entityPtr : scene->GetEntities()) {
			if (!entityPtr) {
				continue;
			}
			Entity* entity = entityPtr.get();
			auto*   transform = entity->GetComponent<TransformComponent>();
			if (!matchesOwner(transform)) {
				continue;
			}

			// プレイヤー直下の CameraRotator を視点ルートとして扱います。
			if (entity->GetComponent<CameraRotatorComponent>() == nullptr) {
				continue;
			}
			mCameraRootEntityGuid = entity->GetGuid();
			return transform;
		}

		return nullptr;
	}

	void ParkourMovementComponent::UpdateDuckViewHeight(
		TransformComponent* actorTransform,
		const MovementFrameInput& input,
		const float         stepSeconds
	) {
		TransformComponent* cameraRoot = ResolveCameraRootTransform(actorTransform);
		if (!cameraRoot) {
			mCameraRootBaseLocalCached         = false;
			mDuckViewSmoothedLocalYInitialized = false;
			return;
		}
		if (!mCameraRootBaseLocalCached) {
			mCameraRootBaseLocalPosition = cameraRoot->GetPosition();
			mCameraRootBaseLocalCached   = true;
		}

		const bool wantsDuckView = input.crouchPressed || mRuntime.duckHullActive ||
		                           mRuntime.slide.active;
		if (wantsDuckView) {
			mDuckViewUnduckHoldRemainingSec = kDuckViewUnduckHoldSec;
		} else {
			mDuckViewUnduckHoldRemainingSec = std::max(
				0.0f,
				mDuckViewUnduckHoldRemainingSec - std::max(stepSeconds, 0.0f)
			);
		}
		const float targetDuckFraction =
			(wantsDuckView || mDuckViewUnduckHoldRemainingSec > 0.0f) ? 1.0f :
			                                                           0.0f;
		if (!mDuckViewFractionInitialized) {
			mDuckViewFraction            = targetDuckFraction;
			mDuckViewFractionInitialized = true;
		}

		const float deltaToTarget = targetDuckFraction - mDuckViewFraction;
		if (std::abs(deltaToTarget) <= 1.0e-5f) {
			mDuckViewFraction = targetDuckFraction;
		} else {
			const bool  ducking = deltaToTarget > 0.0f;
			const float transitionSec = ducking ? mDuckViewTimeToDuckSec :
				                         mDuckViewTimeToUnduckSec;
			if (transitionSec <= 1.0e-6f) {
				mDuckViewFraction = targetDuckFraction;
			} else {
				const float maxStep = std::clamp(
					stepSeconds / transitionSec,
					0.0f,
					1.0f
				);
				const float signedStep = std::copysign(
					std::min(std::abs(deltaToTarget), maxStep),
					deltaToTarget
				);
				mDuckViewFraction = std::clamp(
					mDuckViewFraction + signedStep,
					0.0f,
					1.0f
				);
			}
		}

		const float smoothedFraction = EaseInOutSpline(mDuckViewFraction);
		const float eyeHeightHu      = Math::Lerp(
			mStandViewHeightHu,
			mDuckViewHeightHu,
			smoothedFraction
		);
		const float currentHalfHeightHu = std::max(
			0.0f,
			Math::MtoH(mBoxHalfExtents.y)
		);
		const float targetLocalY = Math::HtoM(eyeHeightHu - currentHalfHeightHu);

		if (!mDuckViewSmoothedLocalYInitialized) {
			mDuckViewSmoothedLocalY            = cameraRoot->GetPosition().y;
			mDuckViewSmoothedLocalYInitialized = true;
		}

		// 視点ローカルYは毎フレーム Lerp で追従させ、姿勢切り替え直後の1フレーム外れを吸収します。
		const float localDelta = targetLocalY - mDuckViewSmoothedLocalY;
		if (std::abs(localDelta) <= 1.0e-6f) {
			mDuckViewSmoothedLocalY = targetLocalY;
		} else {
			const bool  raisingView = localDelta > 0.0f;
			const float transitionSec = raisingView ? mDuckViewTimeToUnduckSec :
				                         mDuckViewTimeToDuckSec;
			const float alpha = transitionSec <= 1.0e-6f ?
				                    1.0f :
				                    std::clamp(stepSeconds / transitionSec, 0.0f, 1.0f);
			mDuckViewSmoothedLocalY = Math::Lerp(
				mDuckViewSmoothedLocalY,
				targetLocalY,
				alpha
			);
		}

		Vec3 localPosition = mCameraRootBaseLocalPosition;
		localPosition.y    = mDuckViewSmoothedLocalY;
		cameraRoot->SetPosition(localPosition);
	}

	bool ParkourMovementComponent::IsDuckDebugDrawEnabled() const {
		return mConsole &&
		       mConsole->GetConVarValueOr("park_duck_debugdraw", true);
	}

	void ParkourMovementComponent::ResetDuckStandDebugFrame() {
		mDuckStandDebug = {};
	}

	void ParkourMovementComponent::DrawDuckStandDebug(
		TransformComponent* transform
	) const {
		if (!IsDuckDebugDrawEnabled() || !transform) {
			return;
		}
		World* world = transform->GetWorld();
		if (!world) {
			return;
		}

		const DuckStandDebugFrame& debug = mDuckStandDebug;
		if (!debug.evaluateStandCalled && !debug.standApplyAttempted &&
		    !debug.duckApplyAttempted) {
			return;
		}

		auto&      debugDraw    = world->GetDebugDraw();
		const Vec4 currentColor = Vec4(1.0f, 0.85f, 0.2f, 1.0f);
		const Vec4 targetColor  = debug.standAllowed ?
			                          Vec4(0.2f, 1.0f, 0.2f, 1.0f) :
			                          Vec4(1.0f, 0.2f, 0.2f, 1.0f);
		const Vec4 pathColor         = Vec4::cyan;
		const Vec4 hitColor          = Vec4::red;
		const Vec4 reachedColor      = Vec4(0.2f, 1.0f, 0.6f, 1.0f);
		const Vec4 headCastColor     = Vec4::orange;
		const Vec4 appliedStandColor = debug.standApplySucceeded ?
			                               Vec4::magenta :
			                               Vec4::darkGray;
		const Vec4 appliedDuckColor = debug.duckApplySucceeded ?
			                              Vec4::blue :
			                              Vec4::darkGray;

		if (debug.evaluateStandCalled) {
			debugDraw.DrawBox(
				debug.currentCenter,
				Quaternion::identity,
				debug.currentHalfExtents * 2.0f,
				currentColor
			);
			debugDraw.DrawBox(
				debug.standTargetCenter,
				Quaternion::identity,
				debug.standTargetHalfExtents * 2.0f,
				targetColor
			);
		}

		const HullOccupancyDebugInfo& occupancy = debug.standOccupancy;
		if (occupancy.checkSweepPath) {
			debugDraw.DrawArrow(
				occupancy.sweepStartCenter,
				occupancy.sweepEndCenter - occupancy.sweepStartCenter,
				pathColor,
				Math::HtoM(4.0f)
			);
			debugDraw.DrawBox(
				occupancy.sweepStartCenter,
				Quaternion::identity,
				occupancy.sweepHalfExtents * 2.0f,
				Vec4::lightGray
			);
			debugDraw.DrawBox(
				occupancy.sweepReachableCenter,
				Quaternion::identity,
				occupancy.sweepHalfExtents * 2.0f,
				reachedColor
			);
			if (occupancy.sweepBlocked) {
				debugDraw.DrawSphere(
					occupancy.sweepHitPos,
					Quaternion::identity,
					Math::HtoM(2.0f),
					hitColor,
					10
				);
				debugDraw.DrawArrow(
					occupancy.sweepHitPos,
					occupancy.sweepHitNormal * Math::HtoM(12.0f),
					hitColor,
					Math::HtoM(2.0f)
				);
			}
		}
		if (occupancy.overlapBlocked) {
			debugDraw.DrawSphere(
				occupancy.overlapHitPos,
				Quaternion::identity,
				Math::HtoM(2.0f),
				hitColor,
				10
			);
			debugDraw.DrawArrow(
				occupancy.overlapHitPos,
				occupancy.overlapHitNormal * Math::HtoM(12.0f),
				hitColor,
				Math::HtoM(2.0f)
			);
		}

		if (debug.headSweepUsed) {
			const Vec3 headCastVec = Vec3::up * debug.headSweepLength;
			debugDraw.DrawArrow(
				debug.headSweepStartCenter,
				headCastVec,
				headCastColor,
				Math::HtoM(4.0f)
			);
			debugDraw.DrawBox(
				debug.headSweepStartCenter,
				Quaternion::identity,
				debug.headSweepHalfExtents * 2.0f,
				headCastColor
			);
			debugDraw.DrawBox(
				debug.headSweepReachableCenter,
				Quaternion::identity,
				debug.headSweepHalfExtents * 2.0f,
				reachedColor
			);
			if (debug.headSweepBlocked) {
				debugDraw.DrawSphere(
					debug.headSweepHitPos,
					Quaternion::identity,
					Math::HtoM(2.0f),
					hitColor,
					10
				);
				debugDraw.DrawArrow(
					debug.headSweepHitPos,
					debug.headSweepHitNormal * Math::HtoM(12.0f),
					hitColor,
					Math::HtoM(2.0f)
				);
			}
		}

		if (debug.standApplyAttempted) {
			debugDraw.DrawBox(
				debug.standAppliedCenter,
				Quaternion::identity,
				debug.standAppliedHalfExtents * 2.0f,
				appliedStandColor
			);
		}
		if (debug.duckApplyAttempted) {
			debugDraw.DrawBox(
				debug.duckAppliedCenter,
				Quaternion::identity,
				debug.duckAppliedHalfExtents * 2.0f,
				appliedDuckColor
			);
		}
	}

	Vec3 ParkourMovementComponent::ComputeHullCenterOffsetForDuckState(
		const MovementContext& context,
		const bool             toDuck
	) const {
		const float deltaHalfY = std::max(
			0.0f,
			mStandingHalfExtents.y - mDuckHalfExtents.y
		);
		if (deltaHalfY <= 0.0f) {
			return Vec3::zero;
		}

		// Source系と同様に、地上では足元固定、空中では頭側固定でハルを切り替えます。
		const bool grounded = IsDuckGrounded(context);
		if (toDuck) {
			return grounded ? -Vec3::up * deltaHalfY : Vec3::up * deltaHalfY;
		}
		return grounded ? Vec3::up * deltaHalfY : -Vec3::up * deltaHalfY;
	}

	bool ParkourMovementComponent::IsDuckGrounded(
		const MovementContext& context
	) const {
		return context.isGrounded ||
		       context.modeState.currentMode == MOVEMENT_MODE_ID::GROUND;
	}

	bool ParkourMovementComponent::CanOccupyHull(
		const MovementContext&  context,
		const Vec3&             targetCenter,
		const Vec3&             targetHalfExtents,
		const bool              checkSweepPath,
		HullOccupancyDebugInfo* outDebugInfo
	) const {
		if (outDebugInfo) {
			*outDebugInfo                  = {};
			outDebugInfo->checkSweepPath   = checkSweepPath;
			outDebugInfo->sweepStartCenter = context.transform ?
				                                 context.transform->
				                                 GetPosition() :
				                                 Vec3::zero;
			outDebugInfo->sweepEndCenter       = targetCenter;
			outDebugInfo->sweepHalfExtents     = targetHalfExtents;
			outDebugInfo->sweepReachableCenter = targetCenter;
		}

		if (!context.transform || !context.resolver) {
			return true;
		}

		const auto* physics = context.resolver->GetPhysics();
		if (!physics) {
			return true;
		}

		const Vec3  startCenter    = context.transform->GetPosition();
		const Vec3  sweepDelta     = targetCenter - startCenter;
		const float sweepDistanceM = sweepDelta.Length();
		const float minSweepM      = Math::HtoM(kDuckSweepMinDistanceHu);

		// 立ち/しゃがみ切り替え時の中心移動をスイープして、経路上の干渉を事前検出します。
		if (checkSweepPath && sweepDistanceM > minSweepM) {
			const Vec3 dir      = sweepDelta / sweepDistanceM;
			const Box  sweepBox = {
				.center   = startCenter,
				.halfSize = targetHalfExtents
			};

			Physics::Hit sweepHit{};
			if (physics->BoxCast(sweepBox, dir, sweepDistanceM, &sweepHit)) {
				if (outDebugInfo) {
					outDebugInfo->sweepHit    = true;
					outDebugInfo->sweepHitToi = std::clamp(
						sweepHit.toi, 0.0f, 1.0f
					);
					outDebugInfo->sweepHitPos          = sweepHit.pos;
					outDebugInfo->sweepHitNormal       = sweepHit.normal;
					outDebugInfo->sweepReachableCenter =
						startCenter + dir * (
							sweepDistanceM * outDebugInfo->sweepHitToi);
				}
				if (sweepHit.startSolid || sweepHit.allsolid ||
				    sweepHit.toi < 1.0f - kDuckSweepBlockToiEpsilon) {
					if (outDebugInfo) {
						outDebugInfo->sweepBlocked = true;
					}
					return false;
				}
			}
		}

		const Box targetBox = {
			.center   = targetCenter,
			.halfSize = targetHalfExtents
		};
		Physics::Hit overlapHit{};
		if (physics->BoxOverlap(targetBox, &overlapHit)) {
			if (outDebugInfo) {
				outDebugInfo->overlapBlocked   = true;
				outDebugInfo->overlapHitPos    = overlapHit.pos;
				outDebugInfo->overlapHitNormal = overlapHit.normal;
			}
			return false;
		}
		return true;
	}

	bool ParkourMovementComponent::ApplyDuckHull(MovementContext& context) {
		if (IsDuckDebugDrawEnabled()) {
			mDuckStandDebug.duckApplyAttempted = true;
			mDuckStandDebug.duckApplySucceeded = false;
			mDuckStandDebug.duckAppliedCenter  = context.transform ?
				                                     context.transform->
				                                     GetPosition() :
				                                     Vec3::zero;
			mDuckStandDebug.duckAppliedHalfExtents = mDuckHalfExtents;
		}

		if (mRuntime.duckHullActive) {
			context.halfExtents = mBoxHalfExtents;
			if (IsDuckDebugDrawEnabled()) {
				mDuckStandDebug.duckApplySucceeded = true;
				mDuckStandDebug.duckAppliedCenter  = context.transform ?
					context.transform->
					        GetPosition() :
					Vec3::zero;
				mDuckStandDebug.duckAppliedHalfExtents = mBoxHalfExtents;
			}
			return true;
		}
		if (!context.transform) {
			return false;
		}

		const Vec3 startCenter  = context.transform->GetPosition();
		const Vec3 targetCenter = startCenter +
		                          ComputeHullCenterOffsetForDuckState(
			                          context, true
		                          );
		const bool grounded = IsDuckGrounded(context);

		// 優先は Source系の地上/空中オフセット。詰まる場合のみ中心据え置きへフォールバックします。
		Vec3 applyCenter = targetCenter;
		if (!CanOccupyHull(
			context, targetCenter, mDuckHalfExtents, !grounded
		)) {
			if (!CanOccupyHull(context, startCenter, mDuckHalfExtents, false)) {
				context.halfExtents = mBoxHalfExtents;
				return false;
			}
			applyCenter = startCenter;
		}
		context.transform->SetPosition(applyCenter);

		mBoxHalfExtents         = mDuckHalfExtents;
		context.halfExtents     = mDuckHalfExtents;
		mRuntime.duckHullActive = true;
		UpdateCollisionHull(context.transform);
		if (IsDuckDebugDrawEnabled()) {
			mDuckStandDebug.duckApplySucceeded     = true;
			mDuckStandDebug.duckAppliedCenter      = applyCenter;
			mDuckStandDebug.duckAppliedHalfExtents = mDuckHalfExtents;
		}
		return true;
	}

	bool ParkourMovementComponent::ApplyStandHull(MovementContext& context) {
		if (IsDuckDebugDrawEnabled()) {
			mDuckStandDebug.standApplyAttempted = true;
			mDuckStandDebug.standApplySucceeded = false;
			mDuckStandDebug.standAppliedCenter  = context.transform ?
				                                      context.transform->
				                                      GetPosition() :
				                                      Vec3::zero;
			mDuckStandDebug.standAppliedHalfExtents = mStandingHalfExtents;
		}

		if (!mRuntime.duckHullActive) {
			context.halfExtents = mBoxHalfExtents;
			if (IsDuckDebugDrawEnabled()) {
				mDuckStandDebug.standApplySucceeded = true;
				mDuckStandDebug.standAppliedCenter  = context.transform ?
					context.transform->GetPosition() :
					Vec3::zero;
				mDuckStandDebug.standAppliedHalfExtents = mBoxHalfExtents;
			}
			return true;
		}
		if (!context.transform) {
			return false;
		}
		if (!CanStandAt(context)) {
			context.halfExtents = mBoxHalfExtents;
			return false;
		}

		const Vec3 standCenter = context.transform->GetPosition() +
		                         ComputeHullCenterOffsetForDuckState(
			                         context, false
		                         );
		context.transform->SetPosition(standCenter);

		mBoxHalfExtents         = mStandingHalfExtents;
		context.halfExtents     = mStandingHalfExtents;
		mRuntime.duckHullActive = false;
		UpdateCollisionHull(context.transform);
		if (IsDuckDebugDrawEnabled()) {
			mDuckStandDebug.standApplySucceeded     = true;
			mDuckStandDebug.standAppliedCenter      = standCenter;
			mDuckStandDebug.standAppliedHalfExtents = mStandingHalfExtents;
		}
		return true;
	}

	REGISTER_COMPONENT(ParkourMovementComponent);
}
