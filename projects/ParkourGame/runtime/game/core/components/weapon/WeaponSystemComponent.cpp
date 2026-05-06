#include "WeaponSystemComponent.h"

#include <algorithm>
#include <cfloat>
#include <imgui.h>

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"
#include "engine/physics/core/Physics.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/world/World.h"

#include "game/core/components/inventory/InventorySystemComponent.h"

namespace Unnamed {
	namespace {
		static constexpr std::string_view kChannel          = "WeaponSystem";
		static constexpr std::string_view kDamageDealtCueId = "damage.dealt";
		static constexpr std::string_view kWeaponFireCueId  = "weapon.fire";

		[[nodiscard]] Vec3 SafeNormalized(
			Vec3       value,
			const Vec3 fallback
		) {
			if (value.IsZero()) {
				return fallback;
			}
			value.Normalize();
			return value;
		}

		[[nodiscard]] Vec3 SafeInvDir(const Vec3& dir) {
			constexpr float epsilon = 1e-6f;
			return Vec3(
				std::abs(dir.x) > epsilon ? 1.0f / dir.x : FLT_MAX,
				std::abs(dir.y) > epsilon ? 1.0f / dir.y : FLT_MAX,
				std::abs(dir.z) > epsilon ? 1.0f / dir.z : FLT_MAX
			);
		}

		[[nodiscard]] bool BuildRay(
			const WeaponActionRuntimeContext& context,
			const float                       rangeMeters,
			Ray&                              outRay
		) {
			// 0 長ベクトルや 0 射程を避ける安全値でレイを構築します。
			const Vec3  dir = SafeNormalized(context.forward, Vec3::forward);
			const float safeRange = std::max(0.01f, rangeMeters);
			outRay = {
				.origin = context.origin,
				.dir    = dir,
				.invDir = SafeInvDir(dir),
				.tMin   = 0.0f,
				.tMax   = safeRange
			};
			return true;
		}

		void DrawTraceLine(
			const WeaponActionRuntimeContext& context,
			const Vec3&                       start,
			const Vec3&                       end,
			const Vec4&                       color
		) {
			if (!context.world || !context.drawDebugTrace) {
				return;
			}
			context.world->GetDebugDraw().DrawLine(start, end, color);
		}

		void PublishWeaponCue(
			const WeaponActionRuntimeContext& context,
			const std::string_view            id,
			const float                       value  = 0.0f,
			const float                       value2 = 0.0f
		) {
			if (!context.world || !context.owner || id.empty()) {
				return;
			}

			GameplayCue cue      = {};
			cue.id               = std::string(id);
			cue.sourceEntityGuid = context.owner->GetGuid();
			cue.value            = value;
			cue.value2           = value2;
			// value/value2 互換を維持しつつ、combat 用の named payload を供給します。
			if (id == kDamageDealtCueId) {
				cue.SetFloat("damageAmount", value);
				cue.SetFloat("hitDistanceMeters", value2);
			} else if (id == kWeaponFireCueId) {
				cue.SetFloat("damageAmount", value);
				cue.SetFloat("executionCount", value2);
			}
			context.world->GetGameplayCueBus().Publish(cue);
			if (context.verboseLogging) {
				DevMsg(
					kChannel,
					"Publish cue id='{}' sourceGuid={} value={:.3f} value2={:.3f}",
					cue.id,
					cue.sourceEntityGuid,
					cue.value,
					cue.value2
				);
			}
		}

		[[nodiscard]] bool WantsPrimaryFire(
			const WeaponActionInput&    input,
			const WeaponItemDefinition& weaponDefinition
		) {
			return weaponDefinition.automatic ?
				       input.primary.held :
				       input.primary.pressed;
		}

		/// @brief 近接アクション用のモジュールです。
		class MeleeWeaponActionModule final : public IWeaponActionModule {
		public:
			[[nodiscard]] std::string_view GetModeId() const override {
				return "melee";
			}

			void Tick(
				const WeaponActionRuntimeContext& context,
				const WeaponActionInput&          input,
				const ItemDefinition&             itemDefinition,
				const WeaponItemDefinition&       weaponDefinition,
				WeaponModuleRuntime&              runtime
			) override {
				if (!input.primary.pressed || runtime.cooldownRemainingSec >
				    0.0f) {
					return;
				}

				runtime.cooldownRemainingSec = std::max(
					0.0f,
					weaponDefinition.fireIntervalSec
				);
				++runtime.executionCount;
				// weapon.fire payload contract:
				// value  = weaponDefinition.damage
				// value2 = 1.0 (single fire execution)
				// named:
				//   payload.damageAmount
				//   payload.executionCount
				PublishWeaponCue(
					context,
					kWeaponFireCueId,
					weaponDefinition.damage,
					1.0f
				);

				const float range = std::max(
					0.1f, weaponDefinition.rangeMeters
				);
				const float radius = std::max(
					0.01f, weaponDefinition.radiusMeters
				);
				const Vec3 dir = SafeNormalized(context.forward, Vec3::forward);
				Vec3       endPos = context.origin + dir * range;
				bool       hitSomething = false;

				if (context.world) {
					Physics::Hit hit = {};
					hitSomething = context.world->GetPhysicsEngine().SphereCast(
						context.origin,
						radius,
						dir,
						range,
						&hit
					);
					if (hitSomething) {
						const float hitDistance = std::clamp(
							                          hit.toi, 0.0f, 1.0f
						                          ) *
						                          range;
						endPos = context.origin + dir * hitDistance;
						// damage.dealt payload contract:
						// value  = weaponDefinition.damage
						// value2 = hit distance (meters)
						// named:
						//   payload.damageAmount
						//   payload.hitDistanceMeters
						PublishWeaponCue(
							context,
							kDamageDealtCueId,
							weaponDefinition.damage,
							hitDistance
						);
						DevMsg(
							kChannel,
							"[{}] melee hit entity={} damage={:.2f}",
							itemDefinition.displayName,
							hit.hitEntityGuid,
							weaponDefinition.damage
						);
					}
				}

				DrawTraceLine(
					context,
					context.origin,
					endPos,
					hitSomething ?
						Vec4(1.0f, 0.2f, 0.2f, 1.0f) :
						Vec4(1.0f, 0.9f, 0.2f, 1.0f)
				);
			}
		};

		/// @brief ヒットスキャン向けモジュールです。
		class HitscanWeaponActionModule final : public IWeaponActionModule {
		public:
			[[nodiscard]] std::string_view GetModeId() const override {
				return "hitscan";
			}

			void Tick(
				const WeaponActionRuntimeContext& context,
				const WeaponActionInput&          input,
				const ItemDefinition&             itemDefinition,
				const WeaponItemDefinition&       weaponDefinition,
				WeaponModuleRuntime&              runtime
			) override {
				if (
					!WantsPrimaryFire(input, weaponDefinition) ||
					runtime.cooldownRemainingSec > 0.0f
				) {
					return;
				}

				runtime.cooldownRemainingSec = std::max(
					0.0f,
					weaponDefinition.fireIntervalSec
				);
				++runtime.executionCount;
				// weapon.fire payload contract:
				// value  = weaponDefinition.damage
				// value2 = 1.0 (single fire execution)
				// named:
				//   payload.damageAmount
				//   payload.executionCount
				PublishWeaponCue(
					context,
					kWeaponFireCueId,
					weaponDefinition.damage,
					1.0f
				);

				Ray ray = {};
				(void)BuildRay(context, weaponDefinition.rangeMeters, ray);

				Vec3 endPos       = ray.origin + ray.dir * ray.tMax;
				bool hitSomething = false;
				if (context.world) {
					Physics::Hit hit = {};
					hitSomething = context.world->GetPhysicsEngine().RayCast(
						ray,
						&hit
					);
					if (hitSomething) {
						const float hitDistance = std::clamp(
							                          hit.toi, 0.0f, 1.0f
						                          ) *
						                          ray.tMax;
						endPos = ray.origin + ray.dir * hitDistance;
						// damage.dealt payload contract:
						// value  = weaponDefinition.damage
						// value2 = hit distance (meters)
						// named:
						//   payload.damageAmount
						//   payload.hitDistanceMeters
						PublishWeaponCue(
							context,
							kDamageDealtCueId,
							weaponDefinition.damage,
							hitDistance
						);
						DevMsg(
							kChannel,
							"[{}] hitscan hit entity={} damage={:.2f}",
							itemDefinition.displayName,
							hit.hitEntityGuid,
							weaponDefinition.damage
						);
					}
				}

				DrawTraceLine(
					context,
					ray.origin,
					endPos,
					hitSomething ?
						Vec4(1.0f, 0.3f, 0.3f, 1.0f) :
						Vec4(0.3f, 0.8f, 1.0f, 1.0f)
				);
			}
		};

		/// @brief プロジェクタイル発生向けモジュールです。
		class ProjectileWeaponActionModule final : public IWeaponActionModule {
		public:
			[[nodiscard]] std::string_view GetModeId() const override {
				return "projectile";
			}

			void Tick(
				const WeaponActionRuntimeContext& context,
				const WeaponActionInput&          input,
				const ItemDefinition&             itemDefinition,
				const WeaponItemDefinition&       weaponDefinition,
				WeaponModuleRuntime&              runtime
			) override {
				if (!input.primary.pressed || runtime.cooldownRemainingSec >
				    0.0f) {
					return;
				}

				runtime.cooldownRemainingSec = std::max(
					0.0f,
					weaponDefinition.fireIntervalSec
				);
				++runtime.executionCount;
				// weapon.fire payload contract:
				// value  = weaponDefinition.damage
				// value2 = 1.0 (single fire execution)
				// named:
				//   payload.damageAmount
				//   payload.executionCount
				PublishWeaponCue(
					context,
					kWeaponFireCueId,
					weaponDefinition.damage,
					1.0f
				);

				const Vec3 dir = SafeNormalized(context.forward, Vec3::forward);
				const Vec3 velocity = dir * std::max(
					                      0.0f,
					                      weaponDefinition.projectileSpeedMps
				                      );
				DrawTraceLine(
					context,
					context.origin,
					context.origin + dir * 1.25f,
					Vec4(0.5f, 0.9f, 0.5f, 1.0f)
				);
				DevMsg(
					kChannel,
					"[{}] projectile spawn requested vel=({:.2f},{:.2f},{:.2f}) damage={:.2f}",
					itemDefinition.displayName,
					velocity.x,
					velocity.y,
					velocity.z,
					weaponDefinition.damage
				);
			}
		};

		/// @brief 設置型（地雷等）アクション向けモジュールです。
		class DeployableWeaponActionModule final : public IWeaponActionModule {
		public:
			[[nodiscard]] std::string_view GetModeId() const override {
				return "deployable";
			}

			void Tick(
				const WeaponActionRuntimeContext& context,
				const WeaponActionInput&          input,
				const ItemDefinition&             itemDefinition,
				const WeaponItemDefinition&       weaponDefinition,
				WeaponModuleRuntime&              runtime
			) override {
				if (!input.primary.pressed && !input.secondary.pressed) {
					return;
				}
				if (runtime.cooldownRemainingSec > 0.0f) {
					return;
				}

				runtime.cooldownRemainingSec = std::max(
					0.0f,
					weaponDefinition.fireIntervalSec
				);
				++runtime.executionCount;
				// weapon.fire payload contract:
				// value  = weaponDefinition.damage
				// value2 = 1.0 (single fire execution)
				// named:
				//   payload.damageAmount
				//   payload.executionCount
				PublishWeaponCue(
					context,
					kWeaponFireCueId,
					weaponDefinition.damage,
					1.0f
				);

				Ray ray = {};
				(void)BuildRay(context, weaponDefinition.rangeMeters, ray);
				Vec3 deployPoint  = ray.origin + ray.dir * ray.tMax;
				Vec3 deployNormal = Vec3::up;

				if (context.world) {
					Physics::Hit hit = {};
					if (context.world->GetPhysicsEngine().RayCast(ray, &hit)) {
						const float hitDistance = std::clamp(
							                          hit.toi, 0.0f, 1.0f
						                          ) *
						                          ray.tMax;
						deployPoint  = ray.origin + ray.dir * hitDistance;
						deployNormal = SafeNormalized(hit.normal, Vec3::up);
					}
				}

				DrawTraceLine(
					context,
					ray.origin,
					deployPoint,
					Vec4(0.9f, 0.6f, 0.2f, 1.0f)
				);
				DevMsg(
					kChannel,
					"[{}] deploy request at ({:.2f},{:.2f},{:.2f}) n=({:.2f},{:.2f},{:.2f})",
					itemDefinition.displayName,
					deployPoint.x,
					deployPoint.y,
					deployPoint.z,
					deployNormal.x,
					deployNormal.y,
					deployNormal.z
				);
			}
		};

		/// @brief ユーティリティ用途モジュールです。
		class UtilityWeaponActionModule final : public IWeaponActionModule {
		public:
			[[nodiscard]] std::string_view GetModeId() const override {
				return "utility";
			}

			void Tick(
				const WeaponActionRuntimeContext& context,
				const WeaponActionInput&          input,
				const ItemDefinition&             itemDefinition,
				const WeaponItemDefinition&       weaponDefinition,
				WeaponModuleRuntime&              runtime
			) override {
				const bool requested = input.primary.pressed ||
				                       input.secondary.pressed ||
				                       input.reloadPressed;
				if (!requested || runtime.cooldownRemainingSec > 0.0f) {
					return;
				}

				runtime.cooldownRemainingSec = std::max(
					0.0f,
					weaponDefinition.fireIntervalSec
				);
				++runtime.executionCount;

				const char* trigger = input.primary.pressed ?
					                      "primary" :
					                      input.secondary.pressed ?
					                      "secondary" :
					                      "reload";
				DevMsg(
					kChannel,
					"[{}] utility trigger={} mode={}",
					itemDefinition.displayName,
					trigger,
					weaponDefinition.modeId
				);
				if (context.verboseLogging) {
					Msg(
						kChannel,
						"[{}] utility activated by {}",
						itemDefinition.displayName,
						trigger
					);
				}
			}
		};

		[[nodiscard]] std::unique_ptr<IWeaponActionModule> CreateModule(
			const std::string& modeId
		) {
			const std::string normalized = StrUtil::ToLowerCase(modeId);
			if (normalized == "melee" || normalized == "punch" ||
			    normalized == "kick") {
				return std::make_unique<MeleeWeaponActionModule>();
			}
			if (normalized == "hitscan" || normalized == "gun" ||
			    normalized == "rifle") {
				return std::make_unique<HitscanWeaponActionModule>();
			}
			if (normalized == "projectile" || normalized == "throw" ||
			    normalized == "throwable") {
				return std::make_unique<ProjectileWeaponActionModule>();
			}
			if (normalized == "deployable" || normalized == "mine" ||
			    normalized == "placed") {
				return std::make_unique<DeployableWeaponActionModule>();
			}
			return std::make_unique<UtilityWeaponActionModule>();
		}
	}

	void IWeaponActionModule::OnSelected(
		const WeaponActionRuntimeContext&,
		const ItemDefinition&,
		const WeaponItemDefinition&,
		WeaponModuleRuntime&
	
	) {}

	void IWeaponActionModule::OnDeselected(
		const WeaponActionRuntimeContext&,
		const ItemDefinition&,
		const WeaponItemDefinition&,
		WeaponModuleRuntime&
	
	) {}

	WeaponSystemComponent::~WeaponSystemComponent() = default;

	void WeaponSystemComponent::EnqueueDeterministicActionInput(
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

	void WeaponSystemComponent::ClearDeterministicActionInputQueue() {
		mDeterministicActionInputQueue.Clear();
	}

	const CharacterActionFrameInput&
	WeaponSystemComponent::GetActionFrameInput() const {
		return mActionFrameInput;
	}

	void WeaponSystemComponent::OnAttached() {
		BaseComponent::OnAttached();
		ClearDeterministicActionInputQueue();
		mActionFrameInput         = {};
		mLastEquippedInstanceGuid = 0;
		mRuntimeByItemInstance.clear();
		// InventorySystem が無い場合は武器実行パイプラインが成立しません。
		const Entity* owner = GetOwner();
		if (owner &&
		    const_cast<Entity*>(owner)->GetComponent<InventorySystemComponent>()
		    ==
		    nullptr) {
			Warning(
				kChannel,
				"Owner entity {} has no InventorySystemComponent; weapon cues will not publish.",
				owner->GetGuid()
			);
		}
		if (mVerboseLogging) {
			DevMsg(
				kChannel,
				"Attached ownerGuid={} (weapon cue source guid).",
				owner ? owner->GetGuid() : 0
			);
		}
	}

	void WeaponSystemComponent::OnDetached() {
		ClearDeterministicActionInputQueue();
		mActionFrameInput         = {};
		mLastEquippedInstanceGuid = 0;
		mRuntimeByItemInstance.clear();
		BaseComponent::OnDetached();
	}

	void WeaponSystemComponent::OnTick(const float deltaTime) {
		// 装備の有無に関係なく、既存ランタイムのクールダウンは毎ティック進めます。
		TickCooldowns(std::max(0.0f, deltaTime));

		// deterministic キューから 1 件だけ入力を消費します。
		DeterministicActionInputPacket packet = {};
		if (!mDeterministicActionInputQueue.Pop(packet)) {
			mActionFrameInput = {};
			return;
		}
		mActionFrameInput = packet.input;

		const InventorySystemComponent* inventory        = nullptr;
		const ItemDefinition*           itemDefinition   = nullptr;
		const ItemInstance*             itemInstance     = nullptr;
		const WeaponItemDefinition*     weaponDefinition = nullptr;
		// 装備武器が解決できない場合はこのティックの実行をスキップします。
		if (!TryResolveEquippedWeapon(
			inventory,
			itemDefinition,
			itemInstance,
			weaponDefinition
		)) {
			if (mVerboseLogging) {
				const Entity* owner = GetOwner();
				DevMsg(
					kChannel,
					"Skip weapon tick: no equipped weapon ownerGuid={}.",
					owner ? owner->GetGuid() : 0
				);
			}
			(void)inventory;
			return;
		}

		const WeaponActionRuntimeContext context = BuildRuntimeContext(
			packet.tick,
			packet.stepSeconds
		);
		TickEquippedWeapon(
			context,
			mActionFrameInput.weapon,
			*itemDefinition,
			*itemInstance,
			*weaponDefinition
		);
	}

	std::string_view WeaponSystemComponent::GetStableName() const {
		return "game.WeaponSystem";
	}

	std::string_view WeaponSystemComponent::GetComponentName() const {
		return "WeaponSystem";
	}

	uint32_t WeaponSystemComponent::GetIcon() const {
		return kIconExplosion;
	}

#ifdef _DEBUG
	void WeaponSystemComponent::DrawInspectorImGui() {
		ImGui::Checkbox("Verbose Logging", &mVerboseLogging);
		ImGui::Checkbox("Draw Debug Trace", &mDrawDebugTrace);
		ImGui::Text(
			"Runtime Items: %d",
			static_cast<int>(mRuntimeByItemInstance.size())
		);
		ImGui::Text(
			"Last Equipped Instance: %llu",
			static_cast<unsigned long long>(mLastEquippedInstanceGuid)
		);
	}
#endif

	void WeaponSystemComponent::Deserialize(const JsonReader& reader) {
		if (const JsonReader verbose = reader["verboseLogging"]; verbose.
			Valid()) {
			mVerboseLogging = verbose.GetBool(mVerboseLogging);
		}
		if (const JsonReader drawTrace = reader["drawDebugTrace"]; drawTrace.
			Valid()) {
			mDrawDebugTrace = drawTrace.GetBool(mDrawDebugTrace);
		}
	}

	void WeaponSystemComponent::Serialize(JsonWriter& writer) const {
		writer.Key("verboseLogging");
		writer.Write(mVerboseLogging);
		writer.Key("drawDebugTrace");
		writer.Write(mDrawDebugTrace);
	}

	WeaponActionRuntimeContext WeaponSystemComponent::BuildRuntimeContext(
		const uint64_t simulationTick,
		const float    stepSeconds
	) const {
		WeaponActionRuntimeContext context = {};
		context.simulationTick             = simulationTick;
		context.stepSeconds                = std::max(0.0f, stepSeconds);
		context.owner                      = GetOwner();
		context.world                      = GetWorld();
		context.drawDebugTrace             = mDrawDebugTrace;
		context.verboseLogging             = mVerboseLogging;

		if (context.owner) {
			if (auto* transform = context.owner->GetComponent<
					TransformComponent>()
			) {
				// 所有者トランスフォームを基準に初期姿勢を作ります。
				context.origin  = transform->GetPosition();
				context.forward = transform->Forward();
				context.right   = transform->Right();
				context.up      = transform->Up();
			}
		}

		if (context.world) {
			// カメラが有効なら視点ベースに上書きして FPS/TPS 両対応を保ちます。
			const auto info = context.world->GetCameraManager().
			                          GetCurrentCameraInfo();
			if (info.valid && info.camera.valid) {
				const Mat4 invView = info.camera.view.Inverse();
				context.origin     = info.camera.cameraPos;
				context.forward    = invView.GetForward();
				context.right      = invView.GetRight();
				context.up         = invView.GetUp();
			}
		}

		context.forward = SafeNormalized(context.forward, Vec3::forward);
		context.right   = SafeNormalized(context.right, Vec3::right);
		context.up      = SafeNormalized(context.up, Vec3::up);
		return context;
	}

	bool WeaponSystemComponent::TryResolveEquippedWeapon(
		const InventorySystemComponent*& outInventory,
		const ItemDefinition*&           outItemDefinition,
		const ItemInstance*&             outItemInstance,
		const WeaponItemDefinition*&     outWeaponDefinition
	) const {
		outInventory        = nullptr;
		outItemDefinition   = nullptr;
		outItemInstance     = nullptr;
		outWeaponDefinition = nullptr;

		const Entity* owner = GetOwner();
		if (!owner) {
			return false;
		}
		auto* inventory = const_cast<Entity*>(owner)->GetComponent<
			InventorySystemComponent>();
		if (!inventory) {
			return false;
		}

		// 装備中アイテムが武器定義を持つ場合のみ実行対象にします。
		const ItemDefinition* itemDefinition = inventory->
			GetEquippedDefinition();
		const ItemInstance* itemInstance = inventory->GetEquippedItem();
		if (!itemDefinition || !itemInstance || !itemDefinition->weapon.
		    has_value()
		) {
			return false;
		}

		outInventory        = inventory;
		outItemDefinition   = itemDefinition;
		outItemInstance     = itemInstance;
		outWeaponDefinition = &itemDefinition->weapon.value();
		return true;
	}

	void WeaponSystemComponent::TickEquippedWeapon(
		const WeaponActionRuntimeContext& context,
		const WeaponActionInput&          input,
		const ItemDefinition&             itemDefinition,
		const ItemInstance&               itemInstance,
		const WeaponItemDefinition&       weaponDefinition
	) {
		// instanceGuid ごとにランタイムを分離し、武器ごとの内部状態を保持します。
		auto& runtime = mRuntimeByItemInstance[itemInstance.instanceGuid];
		if (runtime.modeId != weaponDefinition.modeId || !runtime.module) {
			// モード変更時は旧モジュールを deselect してから新モジュールへ差し替えます。
			if (runtime.module && !runtime.modeId.empty()) {
				WeaponItemDefinition previousDefinition = weaponDefinition;
				previousDefinition.modeId               = runtime.modeId;
				runtime.module->OnDeselected(
					context,
					itemDefinition,
					previousDefinition,
					runtime.runtime
				);
			}
			runtime.modeId  = weaponDefinition.modeId;
			runtime.runtime = {};
			runtime.module  = CreateModule(weaponDefinition.modeId);
			if (!runtime.module) {
				runtime.module = std::make_unique<UtilityWeaponActionModule>();
				Warning(
					kChannel,
					"Failed to create mode '{}'. fallback to utility.",
					weaponDefinition.modeId
				);
			}
			runtime.module->OnSelected(
				context,
				itemDefinition,
				weaponDefinition,
				runtime.runtime
			);
		}

		runtime.module->Tick(
			context,
			input,
			itemDefinition,
			weaponDefinition,
			runtime.runtime
		);
		mLastEquippedInstanceGuid = itemInstance.instanceGuid;
	}

	void WeaponSystemComponent::TickCooldowns(const float deltaTime) {
		for (auto& [instanceGuid, runtime] : mRuntimeByItemInstance) {
			(void)instanceGuid;
			// モジュール側は「0 以下なら実行可能」という前提で扱えるよう 0 で下限固定します。
			runtime.runtime.cooldownRemainingSec = std::max(
				0.0f,
				runtime.runtime.cooldownRemainingSec - deltaTime
			);
		}
	}

	REGISTER_COMPONENT(WeaponSystemComponent);
}
