#include "ParkourMovementAbilities.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>

#include "engine/physics/core/Physics.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/primitive/Primitives.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"
#include "game/core/components/character/state/GameMovementStateMachine.h"
#include "game/core/components/character/state/ability/CoreMovementAbilities.h"
#include "game/core/components/character/state/interface/IMovementAbility.h"
#include "game/parkour/components/character/ParkourMovementComponent.h"

namespace Unnamed {
	namespace {
		constexpr float            kJumpDetachBiasHu         = 1.0f;
		constexpr float            kPathSweepBlockToiEpsilon = 1.0e-4f;
		constexpr std::string_view kVaultLogChannel          = "Parkour/Vault";

		Vec3 HorizontalNormalized(Vec3 value) {
			value.y = 0.0f;
			if (value.IsZero()) {
				return Vec3::zero;
			}
			value.Normalize();
			return value;
		}

		Vec3 GetWishDirHoriz(const MovementContext& context) {
			const Vec3 right   = context.input.right.Normalized();
			const Vec3 forward = context.input.forward.Normalized();
			Vec3       wishDir = right * context.input.moveAxis.x +
			                     forward * context.input.moveAxis.z;
			wishDir.y = 0.0f;
			if (!wishDir.IsZero()) {
				wishDir.Normalize();
			}
			return wishDir;
		}

		void ApplyGroundFriction(
			Vec3&          velocity,
			const float    amount,
			const float    deltaTime,
			ConsoleSystem* console
		) {
			Vec3 velHorz       = velocity;
			velHorz.y          = 0.0f;
			const float speedM = velHorz.Length();
			const float speed  = Math::MtoH(speedM);
			if (speed < 0.1f) {
				return;
			}

			const float stop =
				console ?
					console->GetConVarValueOr(
						"sv_stopspeed", speed
					) :
					speed;
			const float ctrl     = speed < stop ? stop : speed;
			const float drop     = ctrl * amount * deltaTime;
			float       newSpeed = std::max(0.0f, speed - drop);
			if (newSpeed != speed) {
				newSpeed /= speed;
				velocity *= newSpeed;
			}
		}

		void GroundAccelerate(
			Vec3&          currentVel,
			const Vec3     wishDir,
			const float    wishSpeed,
			const float    accel,
			const float    deltaTime,
			ConsoleSystem* console
		) {
			if (wishDir.IsZero() || wishSpeed <= 0.0f || accel <= 0.0f) {
				return;
			}
			const float maxGroundSpeed =
				console ?
					console->GetConVarValueOr("sv_maxspeed", 320.0f) :
					320.0f;
			const float wishSpd           = std::min(wishSpeed, maxGroundSpeed);
			Vec3        currentHorizontal = Math::MtoH(currentVel);
			currentHorizontal.y           = 0.0f;
			const float cur               = currentHorizontal.Dot(wishDir);
			const float add               = wishSpd - cur;
			if (add <= 0.0f) {
				return;
			}
			const float acc = std::min(accel * wishSpd * deltaTime, add);
			currentVel      += Math::HtoM(acc) * wishDir;
		}

		void ExecuteGroundJumpAndSwitchToAir(
			MovementContext& context,
			ConsoleSystem*   console,
			const float      deltaTime
		) {
			if (!context.transform || !context.resolver) {
				return;
			}

			const float detachBiasM =
				Math::HtoM(kJumpDetachBiasHu);

			context.transform->SetPosition(
				context.transform->GetPosition() + Vec3::up * detachBiasM
			);

			context.velocity.y += Math::HtoM(
				console ?
					console->GetConVarValueOr("sv_jumpvelocity", 420.0f) :
					420.0f
			);

			context.jumpSnapDisableRemaining = std::max(
				context.jumpSnapDisableRemaining,
				console ?
					console->GetConVarValueOr("sv_jumpsnapdisabletime", 0.1f) :
					0.1f
			);

			Vec3 jumpPos = context.transform->GetPosition();
			Vec3 jumpVel = context.velocity;
			context.resolver->SlideMove(jumpPos, jumpVel, deltaTime);
			context.transform->SetPosition(jumpPos);
			context.velocity = jumpVel;

			context.isGrounded            = false;
			context.supportEntityGuid     = 0;
			context.supportLinearVelocity = Vec3::zero;
			context.supportStepDelta      = Vec3::zero;
			context.modeTickSuppressed    = true;
			context.SubmitTransition(
				MOVEMENT_MODE_ID::AIR,
				MOVEMENT_TRANSITION_PRIORITY::STANCE,
				"jump from slide",
				"ParkourSlideAbility"
			);
		}

		struct SpeedVaultTrajectory {
			Vec3 startPos    = Vec3::zero;
			Vec3 apexPos     = Vec3::zero;
			Vec3 endPos      = Vec3::zero;
			Vec3 preVelocity = Vec3::zero;
		};

		struct WallRunCandidate {
			Vec3 normal      = Vec3::zero;
			Vec3 direction   = Vec3::zero;
			bool isRightWall = false;
		};

		/// @brief ShapeCast のヒットが移動を妨げるか判定します。
		/// @param hit 判定対象のヒット情報
		/// @return 経路をブロックしている場合は true
		[[nodiscard]] bool IsBlockingCastHit(const Physics::Hit& hit) {
			return hit.startSolid || hit.allsolid ||
			       hit.toi < 1.0f - kPathSweepBlockToiEpsilon;
		}

		/// @brief ハルの移動経路が通行可能かを段階的に検証します。
		/// @param physics 物理エンジン
		/// @param startHull 開始時のハル
		/// @param pathDelta 検証する移動ベクトル
		/// @param stepLengthM 1ステップあたりの最大キャスト長
		/// @return 経路が終点まで到達可能なら true
		[[nodiscard]] bool CanTraverseHullPath(
			const Physics::Engine* physics,
			const Box&             startHull,
			const Vec3&            pathDelta,
			const float            stepLengthM,
			const bool             debugLogEnabled,
			const char*            pathLabel
		) {
			if (!physics) {
				if (debugLogEnabled) {
					DevMsg(
						kVaultLogChannel,
						"[{}] path check failed: physics is null",
						pathLabel
					);
				}
				return false;
			}

			const float travelLengthM = pathDelta.Length();
			const float minStepM      = Math::HtoM(4.0f);
			const float maxStepM      = std::max(stepLengthM, minStepM);
			if (travelLengthM <= 1.0e-6f) {
				return true;
			}

			const Vec3 direction       = pathDelta / travelLengthM;
			float      remainingM      = travelLengthM;
			Vec3       stepStartCenter = startHull.center;
			int        stepIndex       = 0;
			while (remainingM > 1.0e-6f) {
				++stepIndex;
				const float stepM    = std::min(remainingM, maxStepM);
				const Box   stepHull = {
					.center   = stepStartCenter,
					.halfSize = startHull.halfSize
				};
				Physics::Hit sweepHit = {};
				if (physics->BoxCast(stepHull, direction, stepM, &sweepHit) &&
				    IsBlockingCastHit(sweepHit)) {
					if (debugLogEnabled) {
						DevMsg(
							kVaultLogChannel,
							"[{}] path blocked by cast: step={} stepLenM={:.4f} toi={:.4f} startSolid={} allsolid={} hitPos=({:.3f},{:.3f},{:.3f}) hitN=({:.3f},{:.3f},{:.3f})",
							pathLabel,
							stepIndex,
							stepM,
							sweepHit.toi,
							sweepHit.startSolid,
							sweepHit.allsolid,
							sweepHit.pos.x,
							sweepHit.pos.y,
							sweepHit.pos.z,
							sweepHit.normal.x,
							sweepHit.normal.y,
							sweepHit.normal.z
						);
					}
					return false;
				}

				// 長距離スイープの取りこぼしを避けるため、各ステップ終点も Overlap で保証します。
				stepStartCenter       += direction * stepM;
				const Box stepEndHull = {
					.center   = stepStartCenter,
					.halfSize = startHull.halfSize
				};
				Physics::Hit overlapHit = {};
				if (physics->BoxOverlap(stepEndHull, &overlapHit)) {
					if (debugLogEnabled) {
						DevMsg(
							kVaultLogChannel,
							"[{}] path blocked by overlap: step={} endCenter=({:.3f},{:.3f},{:.3f}) hitPos=({:.3f},{:.3f},{:.3f}) hitN=({:.3f},{:.3f},{:.3f})",
							pathLabel,
							stepIndex,
							stepEndHull.center.x,
							stepEndHull.center.y,
							stepEndHull.center.z,
							overlapHit.pos.x,
							overlapHit.pos.y,
							overlapHit.pos.z,
							overlapHit.normal.x,
							overlapHit.normal.y,
							overlapHit.normal.z
						);
					}
					return false;
				}

				remainingM -= stepM;
			}
			return true;
		}

		/// @brief Vault の「崖上面」を複数サンプルの下向きキャストで探索します。
		/// @param physics 物理エンジン
		/// @param feetPos プレイヤー足元位置
		/// @param wallAnchorPos 壁接触近傍のアンカー位置
		/// @param traverseDir Vault の進行方向
		/// @param wallTopHeightM 推定された壁上端高さ（feet 基準）
		/// @param halfWidthM プレイヤー半幅
		/// @param probeSizeM 探索プローブ半径
		/// @param maxVaultHeightM Vault 許容最大高さ
		/// @param debugLogEnabled デバッグログ有効
		/// @param outLedgeGroundHeightM 見つかった上面の地面高さ
		/// @return 上面を見つけたら true
		[[nodiscard]] bool TryFindVaultLedgeSurface(
			const Physics::Engine* physics,
			const Vec3&            feetPos,
			const Vec3&            wallAnchorPos,
			const Vec3&            traverseDir,
			const float            wallTopHeightM,
			const float            halfWidthM,
			const float            probeSizeM,
			const float            maxVaultHeightM,
			const bool             debugLogEnabled,
			float&                 outLedgeGroundHeightM
		) {
			if (!physics || traverseDir.IsZero()) {
				return false;
			}

			const float expectedTopY   = feetPos.y + wallTopHeightM;
			const float probeVerticalM = Math::HtoM(8.0f);
			const float castLengthM    = std::max(
				Math::HtoM(16.0f), maxVaultHeightM + Math::HtoM(16.0f)
			);
			const float maxDropFromTopM = Math::HtoM(24.0f);
			const float forwardStartM   = std::max(halfWidthM, probeSizeM);
			const float forwardEndM     = forwardStartM + std::max(
				                              halfWidthM * 2.0f,
				                              Math::HtoM(24.0f)
			                              );
			const float forwardStepM = std::max(probeSizeM, Math::HtoM(4.0f));
			const float lateralExtentM = std::max(probeSizeM, Math::HtoM(8.0f));
			const float lateralStepM = std::max(probeSizeM, Math::HtoM(4.0f));
			const Vec3  lateralDir = Vec3::up.Cross(traverseDir).Normalized();
			const bool  useLateralSearch = !lateralDir.IsZero();

			int castAttempts = 0;
			int walkableHits = 0;
			for (float forwardOffsetM = forwardStartM;
			     forwardOffsetM <= forwardEndM + 1.0e-6f;
			     forwardOffsetM += forwardStepM) {
				for (float lateralOffsetM = useLateralSearch ?
					                            -lateralExtentM :
					                            0.0f;
				     lateralOffsetM <= (useLateralSearch ?
					                        lateralExtentM + 1.0e-6f :
					                        0.0f);
				     lateralOffsetM += (
					     useLateralSearch ? lateralStepM : 1.0f)) {
					++castAttempts;
					Vec3 probeCenter = wallAnchorPos +
					                   traverseDir * forwardOffsetM +
					                   lateralDir * lateralOffsetM;
					probeCenter.y          = expectedTopY + probeVerticalM;
					const Box surfaceProbe = {
						.center   = probeCenter,
						.halfSize = {
							probeSizeM,
							probeSizeM,
							probeSizeM
						}
					};

					Physics::Hit surfaceHit = {};
					if (!physics->BoxCast(
						surfaceProbe,
						Vec3::down,
						castLengthM,
						&surfaceHit
					)) {
						continue;
					}
					if (surfaceHit.normal.y < 0.7f) {
						continue;
					}
					++walkableHits;

					const float surfaceDistanceM = std::clamp(
						                               surfaceHit.toi, 0.0f,
						                               1.0f
					                               ) * castLengthM;
					const float groundY = probeCenter.y - surfaceDistanceM -
					                      surfaceProbe.halfSize.y;
					if (expectedTopY - groundY > maxDropFromTopM) {
						continue;
					}

					outLedgeGroundHeightM = groundY;
					return true;
				}
			}

			if (debugLogEnabled) {
				DevMsg(
					kVaultLogChannel,
					"ledge surface search failed: attempts={} walkableHits={} expectedTopY={:.3f} anchor=({:.3f},{:.3f},{:.3f}) dir=({:.3f},{:.3f},{:.3f})",
					castAttempts,
					walkableHits,
					expectedTopY,
					wallAnchorPos.x,
					wallAnchorPos.y,
					wallAnchorPos.z,
					traverseDir.x,
					traverseDir.y,
					traverseDir.z
				);
			}
			return false;
		}

		bool TryBuildSpeedVaultTrajectory(
			const MovementContext& context,
			ConsoleSystem*         console,
			SpeedVaultTrajectory&  outTrajectory
		) {
			const bool debugVaultLog = console ?
				                           console->GetConVarValueOr(
					                           "park_vault_debuglog", false
				                           ) :
				                           false;
#define VAULT_FAIL(fmt, ...) \
	do { \
		if (debugVaultLog) { \
			DevMsg(kVaultLogChannel, fmt, ##__VA_ARGS__); \
		} \
		return false; \
	} while (false)

			if (!context.transform || !context.resolver) {
				VAULT_FAIL(
					"reject: missing transform/resolver transform={} resolver={}",
					context.transform != nullptr,
					context.resolver != nullptr
				);
			}

			auto* physics = context.resolver->GetPhysics();
			if (!physics) {
				VAULT_FAIL("reject: physics is null");
			}

			const Vec3 forward = HorizontalNormalized(context.input.forward);
			if (forward.IsZero()) {
				VAULT_FAIL(
					"reject: forward is zero inputForward=({:.3f},{:.3f},{:.3f})",
					context.input.forward.x,
					context.input.forward.y,
					context.input.forward.z
				);
			}

			Vec3 velocityH         = context.velocity;
			velocityH.y            = 0.0f;
			const float minSpeedHu = console ?
				                         console->GetConVarValueOr(
					                         "park_vault_minspeed", 150.0f
				                         ) :
				                         150.0f;
			if (Math::MtoH(velocityH.Length()) < minSpeedHu) {
				VAULT_FAIL(
					"reject: speed too low speedHu={:.2f} minHu={:.2f}",
					Math::MtoH(velocityH.Length()),
					minSpeedHu
				);
			}

			const Vec3  centerPos   = context.transform->GetPosition();
			const float halfWidthM  = context.halfExtents.x;
			const float halfHeightM = context.halfExtents.y;
			if (halfWidthM <= 0.0f || halfHeightM <= 0.0f) {
				VAULT_FAIL(
					"reject: invalid half extents half=({:.4f},{:.4f},{:.4f})",
					context.halfExtents.x,
					context.halfExtents.y,
					context.halfExtents.z
				);
			}
			const float playerHeightM = halfHeightM * 2.0f;
			const Vec3  feetPos       = centerPos - Vec3::up * halfHeightM;

			const float checkDistM = Math::HtoM(
				console ?
					console->GetConVarValueOr(
						"park_vault_forwardcheck", 32.0f
					) :
					32.0f
			);
			const float maxVaultHeightM = Math::HtoM(
				console ?
					console->GetConVarValueOr("park_vault_maxheight", 72.0f) :
					72.0f
			);

			const Box forwardProbe = {
				.center   = feetPos + Vec3::up * (playerHeightM * 0.25f),
				.halfSize = {
					halfWidthM,
					playerHeightM * 0.25f,
					halfWidthM
				}
			};

			Physics::Hit wallHit               = {};
			float        distToWall            = 0.0f;
			bool         wallFound             = false;
			bool         wallDetectedByOverlap = false;
			Vec3         wallNormal            = Vec3::zero;
			Vec3         wallContactPos        = Vec3::zero;
			bool         hasWallContactPos     = false;
			const float  wallCheckCastLength   = checkDistM + halfWidthM;
			if (physics->BoxCast(
				forwardProbe,
				forward,
				wallCheckCastLength,
				&wallHit
			)) {
				wallNormal = wallHit.normal.Normalized();
				if (std::abs(wallNormal.y) > 0.3f) {
					VAULT_FAIL(
						"reject: wall cast normal too vertical wallN=({:.3f},{:.3f},{:.3f})",
						wallNormal.x,
						wallNormal.y,
						wallNormal.z
					);
				}
				// Hit は TOI で返るため、壁までの実距離に変換します。
				distToWall = std::clamp(wallHit.toi, 0.0f, 1.0f) *
				             wallCheckCastLength;
				wallContactPos    = wallHit.pos;
				hasWallContactPos = true;
				wallFound         = true;
			}
			if (!wallFound) {
				Physics::Hit overlapHit = {};
				if (physics->BoxOverlap(forwardProbe, &overlapHit)) {
					wallNormal = overlapHit.normal.Normalized();
					if (std::abs(wallNormal.y) > 0.3f) {
						VAULT_FAIL(
							"reject: wall overlap normal too vertical wallN=({:.3f},{:.3f},{:.3f})",
							wallNormal.x,
							wallNormal.y,
							wallNormal.z
						);
					}
					wallFound             = true;
					wallDetectedByOverlap = true;
				}
			}
			if (!wallFound) {
				VAULT_FAIL(
					"reject: wall not found checkDistM={:.3f} forward=({:.3f},{:.3f},{:.3f})",
					checkDistM,
					forward.x,
					forward.y,
					forward.z
				);
			}
			// Vault の進行方向は入力方向を優先します。
			Vec3 vaultTraverseDir = forward;
			if (vaultTraverseDir.IsZero()) {
				VAULT_FAIL(
					"reject: vault traverse dir is zero wallN=({:.3f},{:.3f},{:.3f})",
					wallNormal.x,
					wallNormal.y,
					wallNormal.z
				);
			}

			float distToWallAlongTraverseM = distToWall;
			bool  hasTraverseDistance      = false;
			{
				// Y 回転した壁では forward 距離をそのまま使うと不足しやすいため、
				// 壁接触点と法線から「進行方向に対する壁距離」を先に見積もります。
				if (!wallDetectedByOverlap && hasWallContactPos) {
					const float denom = wallNormal.Dot(vaultTraverseDir);
					if (std::abs(denom) > 1.0e-4f) {
						const float planeDistance = wallNormal.Dot(
							                            wallContactPos -
							                            forwardProbe.center
						                            ) / denom;
						if (std::isfinite(planeDistance)) {
							distToWallAlongTraverseM = std::max(
								0.0f, planeDistance
							);
							hasTraverseDistance = true;
						}
					}
				}

				Physics::Hit traverseHit          = {};
				const float  traverseCheckLengthM =
					checkDistM + halfWidthM * 2.0f;
				if (physics->BoxCast(
					    forwardProbe,
					    vaultTraverseDir,
					    traverseCheckLengthM,
					    &traverseHit
				    ) && IsBlockingCastHit(traverseHit)) {
					distToWallAlongTraverseM = std::clamp(
						                           traverseHit.toi, 0.0f, 1.0f
					                           ) * traverseCheckLengthM;
					hasTraverseDistance = true;
				} else if (wallDetectedByOverlap) {
					distToWallAlongTraverseM = 0.0f;
					hasTraverseDistance      = true;
				}

				if (!hasTraverseDistance) {
					const Vec3 forwardHitDelta = forward * distToWall;
					distToWallAlongTraverseM   = std::max(
						0.0f, forwardHitDelta.Dot(vaultTraverseDir)
					);
				}
			}

			float       wallTopHeightM = 0.0f;
			bool        foundTop       = false;
			const float probeStepM     = Math::HtoM(8.0f);
			const float probeSizeM     = Math::HtoM(4.0f);
			const float startCheckM    = -Math::HtoM(16.0f);
			for (
				float testHeight = startCheckM;
				testHeight <= maxVaultHeightM + probeStepM;
				testHeight += probeStepM
			) {
				const Box topProbe = {
					.center   = feetPos + Vec3::up * testHeight,
					.halfSize = {
						probeSizeM,
						probeSizeM,
						probeSizeM
					}
				};
				Physics::Hit topHit = {};
				if (!physics->BoxCast(
					topProbe,
					vaultTraverseDir,
					checkDistM + halfWidthM * 2.0f,
					&topHit
				)) {
					wallTopHeightM = testHeight;
					foundTop       = true;
					break;
				}
			}
			if (!foundTop || wallTopHeightM > maxVaultHeightM) {
				VAULT_FAIL(
					"reject: top probe failed foundTop={} wallTopM={:.3f} maxTopM={:.3f}",
					foundTop,
					wallTopHeightM,
					maxVaultHeightM
				);
			}

			float ledgeGroundHeightM = 0.0f;
			{
				Vec3 wallAnchorPos = feetPos +
				                     vaultTraverseDir *
				                     distToWallAlongTraverseM;
				if (hasWallContactPos) {
					wallAnchorPos = wallContactPos;
				}
				wallAnchorPos.y = feetPos.y;
				if (!TryFindVaultLedgeSurface(
					physics,
					feetPos,
					wallAnchorPos,
					vaultTraverseDir,
					wallTopHeightM,
					halfWidthM,
					probeSizeM,
					maxVaultHeightM,
					debugVaultLog,
					ledgeGroundHeightM
				)) {
					VAULT_FAIL(
						"reject: no ledge surface hit anchor=({:.3f},{:.3f},{:.3f}) wallTopM={:.3f}",
						wallAnchorPos.x,
						wallAnchorPos.y,
						wallAnchorPos.z,
						wallTopHeightM
					);
				}
			}

			float wallThicknessM = 0.0f;
			{
				const Vec3 thicknessCheckPos =
					feetPos + Vec3::up * (wallTopHeightM - probeStepM);
				const Box thicknessProbe = {
					.center   = thicknessCheckPos,
					.halfSize = {
						probeSizeM,
						probeSizeM,
						probeSizeM
					}
				};
				Physics::Hit thicknessHit        = {};
				const float  thicknessCastLength = Math::HtoM(256.0f);
				if (physics->BoxCast(
					thicknessProbe,
					vaultTraverseDir,
					thicknessCastLength,
					&thicknessHit
				)) {
					wallThicknessM = std::clamp(thicknessHit.toi, 0.0f, 1.0f) *
					                 thicknessCastLength + probeSizeM * 2.0f;
				} else {
					wallThicknessM = halfWidthM * 2.0f;
				}
			}

			const float overWallOffsetM = std::max(
				distToWallAlongTraverseM + wallThicknessM + halfWidthM +
				Math::HtoM(8.0f),
				halfWidthM * 3.0f + Math::HtoM(8.0f)
			);
			const Vec3 overWallPos =
				feetPos + vaultTraverseDir * overWallOffsetM +
				Vec3::up * (wallTopHeightM + Math::HtoM(4.0f));

			const float routeClearanceM = Math::HtoM(
				console ?
					console->GetConVarValueOr(
						"park_vault_routeclearance", 2.0f
					) :
					2.0f
			);
			const float routeCastStepM = Math::HtoM(
				console ?
					console->GetConVarValueOr(
						"park_vault_routecaststep", 12.0f
					) :
					12.0f
			);
			const float routeTopCenterY = ledgeGroundHeightM + halfHeightM +
			                              std::max(0.0f, routeClearanceM);
			Vec3 verticalRouteTarget = centerPos;
			verticalRouteTarget.y    = std::max(
				verticalRouteTarget.y, routeTopCenterY
			);
			const Box routeHull = {
				.center   = centerPos,
				.halfSize = context.halfExtents
			};
			if (!CanTraverseHullPath(
				physics,
				routeHull,
				verticalRouteTarget - centerPos,
				routeCastStepM,
				debugVaultLog,
				"vertical_route"
			)) {
				VAULT_FAIL(
					"reject: vertical route blocked topCenterY={:.3f} startY={:.3f}",
					verticalRouteTarget.y,
					centerPos.y
				);
			}

			// Overlap 検出起点では壁距離が 0 になりやすいので、最低限の前方経路を保証します。
			const float routeForwardDistanceM = wallDetectedByOverlap ?
				                                    std::max(
					                                    overWallOffsetM,
					                                    halfWidthM * 2.0f +
					                                    Math::HtoM(8.0f)
				                                    ) :
				                                    overWallOffsetM;
			const Box horizontalRouteHull = {
				.center   = verticalRouteTarget,
				.halfSize = context.halfExtents
			};
			if (!CanTraverseHullPath(
				physics,
				horizontalRouteHull,
				vaultTraverseDir * routeForwardDistanceM,
				routeCastStepM,
				debugVaultLog,
				"forward_route"
			)) {
				VAULT_FAIL(
					"reject: forward route blocked distM={:.3f} dir=({:.3f},{:.3f},{:.3f})",
					routeForwardDistanceM,
					vaultTraverseDir.x,
					vaultTraverseDir.y,
					vaultTraverseDir.z
				);
			}

			const Box landingProbe = {
				.center   = overWallPos,
				.halfSize = {
					halfWidthM,
					Math::HtoM(2.0f),
					halfWidthM
				}
			};
			Physics::Hit landHit        = {};
			const float  dropCheckDistM = std::max(
				maxVaultHeightM + Math::HtoM(32.0f),
				Math::HtoM(
					console ?
						console->GetConVarValueOr(
							"park_vault_landingcheck", 72.0f
						) :
						72.0f
				)
			);
			if (!physics->BoxCast(
				landingProbe,
				Vec3::down,
				dropCheckDistM,
				&landHit
			)) {
				VAULT_FAIL(
					"reject: landing cast miss dropCheckM={:.3f} landingProbeY={:.3f}",
					dropCheckDistM,
					landingProbe.center.y
				);
			}
			if (landHit.normal.y < 0.7f) {
				VAULT_FAIL(
					"reject: landing surface too steep normalY={:.3f}",
					landHit.normal.y
				);
			}

			const float landingDistanceM = std::clamp(landHit.toi, 0.0f, 1.0f) *
			                               dropCheckDistM;
			Vec3 landingFeetPos = overWallPos + Vec3::down * landingDistanceM;
			landingFeetPos.y    = std::max(landingFeetPos.y, feetPos.y);

			const Box landingHull = {
				.center   = landingFeetPos + Vec3::up * halfHeightM,
				.halfSize = context.halfExtents
			};
			Physics::Hit overlapHit = {};
			if (physics->BoxOverlap(landingHull, &overlapHit)) {
				VAULT_FAIL(
					"reject: landing hull overlap endCenter=({:.3f},{:.3f},{:.3f}) hitPos=({:.3f},{:.3f},{:.3f})",
					landingHull.center.x,
					landingHull.center.y,
					landingHull.center.z,
					overlapHit.pos.x,
					overlapHit.pos.y,
					overlapHit.pos.z
				);
			}

			{
				const Box backCheckProbe = {
					.center   = landingFeetPos + Vec3::up * halfHeightM,
					.halfSize = {
						probeSizeM,
						halfHeightM,
						probeSizeM
					}
				};
				Physics::Hit backHit = {};
				if (physics->BoxCast(
					backCheckProbe,
					-vaultTraverseDir,
					overWallOffsetM,
					&backHit
				)) {
					if (backHit.normal.Dot(vaultTraverseDir) > 0.3f) {
						VAULT_FAIL(
							"reject: back check blocked dot={:.3f} hitN=({:.3f},{:.3f},{:.3f})",
							backHit.normal.Dot(vaultTraverseDir),
							backHit.normal.x,
							backHit.normal.y,
							backHit.normal.z
						);
					}
				}
			}

			const float apexForwardM = std::max(
				halfWidthM,
				distToWallAlongTraverseM * 0.5f
			);
			const Vec3 apexFeetPos =
				feetPos + vaultTraverseDir * apexForwardM +
				Vec3::up * (wallTopHeightM + Math::HtoM(8.0f));

			outTrajectory.startPos    = centerPos;
			outTrajectory.apexPos     = apexFeetPos + Vec3::up * halfHeightM;
			outTrajectory.endPos      = landingFeetPos + Vec3::up * halfHeightM;
			outTrajectory.preVelocity = context.velocity;
#undef VAULT_FAIL
			return true;
		}

		bool TryBuildWallRunCandidate(
			const MovementContext&                          context,
			ConsoleSystem*                                  console,
			const ParkourMovementComponent::ParkourRuntime& runtime,
			WallRunCandidate&                               outCandidate
		) {
			if (!context.transform || !context.resolver) {
				return false;
			}
			const auto* physics = context.resolver->GetPhysics();
			if (!physics) {
				return false;
			}

			Vec3 velHorz          = context.velocity;
			velHorz.y             = 0.0f;
			const float minSpeedM = Math::HtoM(
				console ?
					console->GetConVarValueOr("park_wallrun_minspeed", 200.0f) :
					200.0f
			);
			if (velHorz.SqrLength() < minSpeedM * minSpeedM) {
				return false;
			}

			const float cooldown = console ?
				                       console->GetConVarValueOr(
					                       "park_wallrun_cooldown", 0.1f
				                       ) :
				                       0.1f;
			if (runtime.wallRun.timeSinceLast < cooldown) {
				return false;
			}

			Vec3 forward = HorizontalNormalized(context.input.forward);
			if (forward.IsZero()) {
				return false;
			}

			Vec3 right = HorizontalNormalized(context.input.right);
			if (right.IsZero()) {
				right = Vec3::up.Cross(forward).Normalized();
				if (right.IsZero()) {
					return false;
				}
			}

			const float sideCheckDistance = context.halfExtents.x + Math::HtoM(
				                                console ?
					                                console->GetConVarValueOr(
						                                "park_wallrun_checkdistance",
						                                10.0f
					                                ) :
					                                10.0f
			                                );
			const std::array<Vec3, 2> checkDirections = {right, -right};
			for (const Vec3& checkDir : checkDirections) {
				Physics::Hit hit{};
				const Box    probe = {
					.center   = context.transform->GetPosition(),
					.halfSize = context.halfExtents
				};
				if (!physics->BoxCast(
					probe, checkDir, sideCheckDistance, &hit
				)) {
					continue;
				}

				const Vec3 wallNormal = hit.normal.Normalized();
				if (wallNormal.IsZero() || std::abs(wallNormal.y) > 0.2f) {
					continue;
				}

				if (!velHorz.IsZero()) {
					const Vec3  velDir = velHorz.Normalized();
					const float dot    = std::abs(velDir.Dot(wallNormal));
					const float maxDot = console ?
						                     console->GetConVarValueOr(
							                     "park_wallrun_maxnormaldot",
							                     0.5f
						                     ) :
						                     0.5f;
					if (dot > maxDot) {
						continue;
					}
				}

				const float sameWallCooldown = console ?
					                               console->GetConVarValueOr(
						                               "park_wallrun_samewallcooldown",
						                               1.0f
					                               ) :
					                               1.0f;
				if (
					runtime.wallRun.timeSinceLast < sameWallCooldown &&
					wallNormal.Dot(runtime.wallRun.lastWallNormal) > 0.9f
				) {
					continue;
				}

				Vec3 along = Vec3::up.Cross(wallNormal).Normalized();
				if (along.Dot(forward) < 0.0f) {
					along = -along;
				}
				outCandidate.normal      = wallNormal;
				outCandidate.direction   = along;
				outCandidate.isRightWall = checkDir.Dot(right) > 0.0f;
				return true;
			}

			return false;
		}

		ParkourMovementComponent* AsParkour(MovementContext& context) {
			return dynamic_cast<ParkourMovementComponent*>(
				context.movementComponent
			);
		}

		const ParkourMovementComponent* AsParkour(
			const MovementContext& context
		) {
			return dynamic_cast<const ParkourMovementComponent*>(
				context.movementComponent
			);
		}

		class ParkourCrouchAbility final : public IMovementAbility {
		public:
			void Init(ConsoleSystem* console) override {
				mConsole = console;
			}

			[[nodiscard]] MOVEMENT_ABILITY_SLOT GetSlot() const override {
				return MOVEMENT_ABILITY_SLOT::CROUCH;
			}

			[[nodiscard]] std::string_view GetDebugName() const override {
				return "parkour_crouch";
			}

			[[nodiscard]] bool IsForcedAbility() const override {
				return false;
			}

			[[nodiscard]] bool CanRunInMode(MOVEMENT_MODE_ID) const override {
				return true;
			}

			[[nodiscard]] bool CanStart(
				const MovementContext& context
			) const override {
				const auto* parkour = dynamic_cast<const
					ParkourMovementComponent*>(
					context.movementComponent
				);
				return context.input.crouchPressed ||
				       (parkour && !parkour->CanStandAt(context));
			}

			bool Start(MovementContext& context, float) override {
				if (auto* parkour = AsParkour(context)) {
					(void)parkour->SetDuckHullEnabled(context, true);
				}
				return true;
			}

			bool Tick(MovementContext& context, float) override {
				auto* parkour = AsParkour(context);
				if (!parkour) {
					return context.input.crouchPressed;
				}
				(void)parkour->SetDuckHullEnabled(context, true);
				if (context.input.crouchPressed || !parkour->CanStandAt(context)
				    ||
				    context.IsAbilityActive(MOVEMENT_ABILITY_SLOT::SLIDE)) {
					return true;
				}
				// 解除要求があっても立てない場合は crouch を維持します。
				return !parkour->SetDuckHullEnabled(context, false);
			}

			void Stop(MovementContext& context) override {
				if (auto* parkour = AsParkour(context)) {
					(void)parkour->SetDuckHullEnabled(context, false);
				}
			}
		};

		class ParkourDoubleJumpAbility final : public IMovementAbility {
		public:
			void Init(ConsoleSystem* console) override {
				mConsole = console;
			}

			[[nodiscard]] MOVEMENT_ABILITY_SLOT GetSlot() const override {
				return MOVEMENT_ABILITY_SLOT::DOUBLE_JUMP;
			}

			[[nodiscard]] std::string_view GetDebugName() const override {
				return "parkour_doublejump";
			}

			[[nodiscard]] bool IsForcedAbility() const override {
				return false;
			}

			[[nodiscard]] bool
			CanRunInMode(MOVEMENT_MODE_ID modeId) const override {
				return modeId == MOVEMENT_MODE_ID::AIR;
			}

			[[nodiscard]] bool CanStart(
				const MovementContext& context
			) const override {
				return AsParkour(context) != nullptr &&
				       context.modeState.currentMode == MOVEMENT_MODE_ID::AIR &&
				       !context.IsAbilityActive(
					       MOVEMENT_ABILITY_SLOT::WALL_RUN
				       ) &&
				       !context.IsAbilityActive(MOVEMENT_ABILITY_SLOT::BLINK) &&
				       !context.IsAbilityActive(
					       MOVEMENT_ABILITY_SLOT::SPEED_VAULT
				       );
			}

			bool Start(MovementContext& context, float) override {
				if (auto* parkour = AsParkour(context)) {
					parkour->GetParkourRuntime().lastJumpHeld = context.input.
						jumpPressed;
					return true;
				}
				return false;
			}

			bool Tick(MovementContext& context, float) override {
				auto* parkour = AsParkour(context);
				if (!parkour || context.modeState.currentMode !=
				    MOVEMENT_MODE_ID::AIR) {
					return false;
				}
				auto& runtime = parkour->GetParkourRuntime();
				if (context.IsAbilityActive(MOVEMENT_ABILITY_SLOT::WALL_RUN) ||
				    context.IsAbilityActive(MOVEMENT_ABILITY_SLOT::BLINK) ||
				    context.IsAbilityActive(
					    MOVEMENT_ABILITY_SLOT::SPEED_VAULT
				    )) {
					// 特殊移動中も入力エッジ同期だけは維持し、解除直後の誤消費を防ぎます。
					runtime.lastJumpHeld = context.input.jumpPressed;
					return true;
				}
				const bool jumpEdge =
					context.input.jumpPressed && !runtime.lastJumpHeld;
				if (jumpEdge && runtime.hasDoubleJump) {
					context.velocity.y = Math::HtoM(
						mConsole ?
							mConsole->GetConVarValueOr(
								"park_doublejump_velocity", 300.0f
							) :
							300.0f
					);
					runtime.hasDoubleJump            = false;
					runtime.pendingDoubleJumpCue     = true;
					context.jumpSnapDisableRemaining = std::max(
						context.jumpSnapDisableRemaining,
						mConsole ?
							mConsole->GetConVarValueOr(
								"sv_jumpsnapdisabletime", 0.1f
							) :
							0.1f
					);
				}
				runtime.lastJumpHeld = context.input.jumpPressed;
				return true;
			}

			void Stop(MovementContext& context) override {
				if (auto* parkour = AsParkour(context)) {
					parkour->GetParkourRuntime().lastJumpHeld = context.input.
						jumpPressed;
				}
			}
		};

		class ParkourBlinkAbility final : public IMovementAbility {
		public:
			void Init(ConsoleSystem* console) override {
				mConsole = console;
			}

			[[nodiscard]] MOVEMENT_ABILITY_SLOT GetSlot() const override {
				return MOVEMENT_ABILITY_SLOT::BLINK;
			}

			[[nodiscard]] std::string_view GetDebugName() const override {
				return "parkour_blink";
			}

			[[nodiscard]] bool IsForcedAbility() const override {
				return true;
			}

			[[nodiscard]] bool CanRunInMode(MOVEMENT_MODE_ID) const override {
				return true;
			}

			[[nodiscard]] bool CanStart(
				const MovementContext& context
			) const override {
				const auto* parkour = dynamic_cast<const
					ParkourMovementComponent*>(
					context.movementComponent
				);
				if (!parkour || !context.transform || !context.resolver) {
					return false;
				}
				const auto& runtime = parkour->GetParkourRuntime();
				return context.input.sprintPressed && !runtime.blink.active &&
				       !runtime.vault.active &&
				       runtime.blink.cooldown <= 0.0f;
			}

			bool Start(MovementContext& context, float) override {
				auto* parkour = AsParkour(context);
				if (!parkour || !context.transform || !context.resolver) {
					return false;
				}

				Vec3 dir = context.input.forward.Normalized();
				if (dir.IsZero()) {
					return false;
				}

				const float distanceHu = mConsole ?
					                         mConsole->GetConVarValueOr(
						                         "park_blink_distance", 512.0f
					                         ) :
					                         512.0f;
				const Vec3 startPos       = context.transform->GetPosition();
				Vec3       targetPos      = startPos;
				Vec3       pseudoVelocity = dir * Math::HtoM(distanceHu);
				context.resolver->SlideMove(targetPos, pseudoVelocity, 1.0f);

				const Vec3 blinkDelta = targetPos - startPos;
				if (blinkDelta.SqrLength() <= 1.0e-10f) {
					return false;
				}

				const float verticalSpeed      = context.velocity.y;
				Vec3        horizontalVelocity = context.velocity;
				horizontalVelocity.y           = 0.0f;
				const float horizontalSpeed    = horizontalVelocity.Length();
				Vec3        horizontalDelta    = blinkDelta;
				horizontalDelta.y              = 0.0f;
				if (!horizontalDelta.IsZero() && horizontalSpeed > 0.0f) {
					horizontalDelta.Normalize();
					context.velocity   = horizontalDelta * horizontalSpeed;
					context.velocity.y = verticalSpeed;
				}

				auto& runtime           = parkour->GetParkourRuntime();
				runtime.blink.active    = true;
				runtime.blink.moveTime  = 0.0f;
				runtime.blink.startPos  = startPos;
				runtime.blink.targetPos = targetPos;
				runtime.blink.cooldown  = mConsole ?
					                          mConsole->GetConVarValueOr(
						                          "park_blink_cooldown", 1.0f
					                          ) :
					                          1.0f;
				context.isGrounded               = false;
				context.supportEntityGuid        = 0;
				context.supportLinearVelocity    = Vec3::zero;
				context.supportStepDelta         = Vec3::zero;
				context.jumpSnapDisableRemaining = std::max(
					context.jumpSnapDisableRemaining,
					mConsole ?
						mConsole->GetConVarValueOr(
							"sv_jumpsnapdisabletime", 0.1f
						) :
						0.1f
				);
				context.modeTickSuppressed = true;
				context.SubmitTransition(
					MOVEMENT_MODE_ID::AIR,
					MOVEMENT_TRANSITION_PRIORITY::FORCED_ABILITY,
					"blink start",
					"ParkourBlinkAbility"
				);
				return true;
			}

			bool Tick(MovementContext& context, float deltaTime) override {
				auto* parkour = AsParkour(context);
				if (!parkour || !context.transform) {
					return false;
				}
				auto& runtime = parkour->GetParkourRuntime();
				if (!runtime.blink.active) {
					return false;
				}
				context.modeTickSuppressed = true;
				const float duration       = std::max(
					1.0e-4f,
					mConsole ?
						mConsole->GetConVarValueOr(
							"park_blink_moveduration", 0.08f
						) :
						0.08f
				);
				runtime.blink.moveTime += std::max(0.0f, deltaTime);
				const float t          = std::clamp(
					runtime.blink.moveTime / duration, 0.0f, 1.0f
				);
				context.transform->SetPosition(
					Math::Lerp(
						runtime.blink.startPos, runtime.blink.targetPos, t
					)
				);
				parkour->SyncCollisionHull(context.transform);
				context.isGrounded            = false;
				context.supportEntityGuid     = 0;
				context.supportLinearVelocity = Vec3::zero;
				context.supportStepDelta      = Vec3::zero;
				if (t >= 1.0f) {
					runtime.blink.active = false;
					return true;
				}
				return true;
			}

			void Stop(MovementContext& context) override {
				if (auto* parkour = AsParkour(context)) {
					parkour->GetParkourRuntime().blink.active = false;
				}
			}
		};

		class ParkourWallRunAbility final : public IMovementAbility {
		public:
			void Init(ConsoleSystem* console) override {
				mConsole = console;
			}

			[[nodiscard]] MOVEMENT_ABILITY_SLOT GetSlot() const override {
				return MOVEMENT_ABILITY_SLOT::WALL_RUN;
			}

			[[nodiscard]] std::string_view GetDebugName() const override {
				return "parkour_wallrun";
			}

			[[nodiscard]] bool IsForcedAbility() const override {
				return false;
			}

			[[nodiscard]] bool
			CanRunInMode(MOVEMENT_MODE_ID modeId) const override {
				return modeId == MOVEMENT_MODE_ID::AIR;
			}

			[[nodiscard]] bool CanStart(
				const MovementContext& context
			) const override {
				const auto* parkour = dynamic_cast<const
					ParkourMovementComponent*>(
					context.movementComponent
				);
				if (!parkour || !context.resolver || !context.transform) {
					return false;
				}
				if (context.isGrounded || context.input.crouchPressed ||
				    parkour->GetParkourRuntime().wallRun.active) {
					return false;
				}
				WallRunCandidate candidate = {};
				return TryBuildWallRunCandidate(
					context,
					mConsole,
					parkour->GetParkourRuntime(),
					candidate
				);
			}

			bool Start(MovementContext& context, float) override {
				auto* parkour = AsParkour(context);
				if (!parkour) {
					return false;
				}

				WallRunCandidate candidate = {};
				if (!TryBuildWallRunCandidate(
					context,
					mConsole,
					parkour->GetParkourRuntime(),
					candidate
				)) {
					return false;
				}

				auto& runtime = parkour->GetParkourRuntime();
				runtime.wallRun.active = true;
				runtime.wallRun.normal = candidate.normal;
				runtime.wallRun.time = 0.0f;
				runtime.wallRun.jumpWasHeldOnInit = context.input.jumpPressed;
				runtime.wallRun.direction = candidate.direction.IsZero() ?
					                            Vec3::forward :
					                            candidate.direction;
				runtime.hasDoubleJump = true;
				runtime.slide.active  = false;

				const float originalY = context.velocity.y;
				Vec3        velHorz = context.velocity;
				velHorz.y = 0.0f;
				const float currentSpeed = velHorz.Length();
				const float alongSpeed = velHorz.Dot(runtime.wallRun.direction);
				if (std::abs(alongSpeed) > 1.0e-3f) {
					context.velocity =
						runtime.wallRun.direction * std::abs(alongSpeed);
				} else {
					context.velocity = runtime.wallRun.direction * currentSpeed;
				}

				const float verticalDamping = mConsole ?
					                              mConsole->GetConVarValueOr(
						                              "park_wallrun_verticaldamping",
						                              0.3f
					                              ) :
					                              0.3f;
				if (originalY > 0.0f) {
					context.velocity.y = originalY * verticalDamping;
				} else if (originalY < 0.0f) {
					context.velocity.y = originalY * 0.3f;
				}

				const float startBoost = mConsole ?
					                         mConsole->GetConVarValueOr(
						                         "park_wallrun_startboost",
						                         50.0f
					                         ) :
					                         50.0f;
				context.velocity += runtime.wallRun.direction * Math::HtoM(
					startBoost
				);
				context.modeTickSuppressed = true;
				parkour->PublishParkourMovementCue(
					candidate.isRightWall ?
						"movement.wallrun.start.right" :
						"movement.wallrun.start.left"
				);
				return true;
			}

			bool Tick(MovementContext& context, float deltaTime) override {
				auto* parkour = AsParkour(context);
				if (!parkour || !context.transform || !context.resolver) {
					return false;
				}
				auto& runtime = parkour->GetParkourRuntime();
				if (!runtime.wallRun.active) {
					return false;
				}
				// 旧State互換: 壁走り中は毎tickでジャンプ保持状態を同期します。
				runtime.lastJumpHeld       = context.input.jumpPressed;
				context.modeTickSuppressed = true;

				const auto* physics = context.resolver->GetPhysics();
				if (!physics) {
					parkour->EndWallRun();
					return false;
				}

				runtime.wallRun.time += std::max(0.0f, deltaTime);
				const float maxTime  = mConsole ?
					                       mConsole->GetConVarValueOr(
						                       "park_wallrun_maxtime", 2.5f
					                       ) :
					                       2.5f;
				if (runtime.wallRun.time >= maxTime) {
					parkour->EndWallRun();
					return false;
				}

				const float maintainDistance =
					context.halfExtents.x + Math::HtoM(
						mConsole ?
							mConsole->GetConVarValueOr(
								"park_wallrun_maintaindistance", 20.0f
							) :
							20.0f
					);
				Physics::Hit wallHit = {};
				const Box    probe   = {
					.center   = context.transform->GetPosition(),
					.halfSize = context.halfExtents
				};
				if (!physics->BoxCast(
					probe,
					-runtime.wallRun.normal,
					maintainDistance,
					&wallHit
				)) {
					parkour->EndWallRun();
					return false;
				}

				const Vec3 newNormal = wallHit.normal.Normalized();
				if (newNormal.IsZero() || newNormal.Dot(
					    runtime.wallRun.normal
				    ) < 0.5f) {
					parkour->EndWallRun();
					return false;
				}
				runtime.wallRun.normal = (
						runtime.wallRun.normal * 0.8f + newNormal * 0.2f).
					Normalized();

				Vec3 projectedDir = Math::ProjectOnPlane(
					HorizontalNormalized(context.input.forward),
					runtime.wallRun.normal
				);
				if (!projectedDir.IsZero()) {
					projectedDir.Normalize();
					runtime.wallRun.direction = projectedDir;
				}

				if (context.input.crouchPressed || context.isGrounded) {
					parkour->EndWallRun();
					return false;
				}

				Vec3 velHorz           = context.velocity;
				velHorz.y              = 0.0f;
				const float minSpeedHu = mConsole ?
					                         mConsole->GetConVarValueOr(
						                         "park_wallrun_minspeed", 200.0f
					                         ) :
					                         200.0f;
				if (Math::MtoH(velHorz.Length()) < minSpeedHu * 0.5f) {
					parkour->EndWallRun();
					return false;
				}

				const bool detachOnSideInput = mConsole ?
					                               mConsole->GetConVarValueOr(
						                               "park_wallrun_detachonsideinput",
						                               true
					                               ) :
					                               true;
				if (detachOnSideInput && std::abs(
					    context.input.moveAxis.x
				    ) > 0.5f) {
					const Vec3 camRight = HorizontalNormalized(
						context.input.right
					);
					if (!camRight.IsZero()) {
						const float wallSide = camRight.Dot(
							runtime.wallRun.normal
						);
						if (
							(wallSide > 0.0f && context.input.moveAxis.x > 0.5f)
							||
							(wallSide < 0.0f && context.input.moveAxis.x < -
							 0.5f)
						) {
							parkour->EndWallRun();
							return false;
						}
					}
				}

				if (context.input.jumpPressed) {
					if (!runtime.wallRun.jumpWasHeldOnInit) {
						const float jumpForceHu = mConsole ?
							                          mConsole->
							                          GetConVarValueOr(
								                          "park_wallrun_jumpforce",
								                          350.0f
							                          ) :
							                          350.0f;
						const Vec3 forwardVel = runtime.wallRun.direction *
						                        context.velocity.Dot(
							                        runtime.wallRun.direction
						                        );
						Vec3 awayDir = runtime.wallRun.normal * 0.7f + Vec3::up;
						awayDir.Normalize();
						context.velocity = forwardVel + awayDir * Math::HtoM(
							                   jumpForceHu
						                   );
						runtime.hasDoubleJump = true;
						parkour->PublishParkourMovementCue(
							"movement.wallrun.jump"
						);
						context.jumpSnapDisableRemaining = std::max(
							context.jumpSnapDisableRemaining,
							mConsole ?
								mConsole->GetConVarValueOr(
									"sv_jumpsnapdisabletime", 0.1f
								) :
								0.1f
						);
						parkour->EndWallRun();
						context.SubmitTransition(
							MOVEMENT_MODE_ID::AIR,
							MOVEMENT_TRANSITION_PRIORITY::STANCE,
							"wallrun jump",
							"ParkourWallRunAbility"
						);
						return false;
					}
				} else {
					runtime.wallRun.jumpWasHeldOnInit = false;
				}

				context.isGrounded            = false;
				context.supportEntityGuid     = 0;
				context.supportLinearVelocity = Vec3::zero;
				context.supportStepDelta      = Vec3::zero;

				const float gravity = mConsole ?
					                      mConsole->GetConVarValueOr(
						                      "sv_gravity", 800.0f
					                      ) :
					                      800.0f;
				const float gravityScale = mConsole ?
					                           mConsole->GetConVarValueOr(
						                           "park_wallrun_gravityscale",
						                           0.1f
					                           ) :
					                           0.1f;
				context.velocity.y -= Math::HtoM(gravity) * gravityScale *
					deltaTime;

				const float wishSpeed = mConsole ?
					                        mConsole->GetConVarValueOr(
						                        "sv_sprintspeed", 320.0f
					                        ) :
					                        320.0f;
				if (context.input.moveAxis.z > 0.0f && !runtime.wallRun.
				    direction.IsZero()) {
					const float currentSpeed = Math::MtoH(
						context.velocity.Dot(runtime.wallRun.direction)
					);
					const float addSpeed = wishSpeed * 1.2f - currentSpeed;
					if (addSpeed > 0.0f) {
						const float accel = (mConsole ?
							                     mConsole->GetConVarValueOr(
								                     "sv_airaccelerate", 10.0f
							                     ) :
							                     10.0f) *
						                    1.5f;
						const float accelSpeed = std::min(
							accel * wishSpeed * deltaTime,
							addSpeed
						);
						context.velocity +=
							Math::HtoM(accelSpeed) * runtime.wallRun.direction;
					}
				} else {
					const float speedM = context.velocity.Length();
					const float speed  = Math::MtoH(speedM);
					if (speed > 0.1f) {
						const float friction = mConsole ?
							                       mConsole->GetConVarValueOr(
								                       "sv_friction", 5.2f
							                       ) :
							                       5.2f;
						const float drop = speed * friction * deltaTime * 0.5f;
						const float nextSpeed = std::max(0.0f, speed - drop);
						if (nextSpeed != speed) {
							context.velocity *= (nextSpeed / speed);
						}
					}
				}

				const float intoWall = context.velocity.Dot(
					-runtime.wallRun.normal
				);
				if (intoWall > 0.0f) {
					context.velocity += runtime.wallRun.normal * intoWall;
				}
				const float pullForce = Math::HtoM(
					mConsole ?
						mConsole->GetConVarValueOr(
							"park_wallrun_pullforce", 80.0f
						) :
						80.0f
				);
				context.velocity += -runtime.wallRun.normal * pullForce *
					deltaTime;

				Vec3 movePos = context.transform->GetPosition();
				Vec3 moveVel = context.velocity;
				context.resolver->SlideMove(movePos, moveVel, deltaTime);
				context.transform->SetPosition(movePos);
				context.velocity = moveVel;

				Physics::Hit groundHit = {};
				if (context.velocity.y <= 0.0f &&
				    context.resolver->ProbeGround(
					    movePos,
					    Math::HtoM(2.0f),
					    &groundHit
				    ) &&
				    (groundHit.startSolid || groundHit.allsolid ||
				     groundHit.normal.y > 0.7f)) {
					context.velocity.y        = 0.0f;
					context.isGrounded        = true;
					context.supportEntityGuid = groundHit.hitEntityGuid;
					parkour->EndWallRun();
					context.SubmitTransition(
						MOVEMENT_MODE_ID::GROUND,
						MOVEMENT_TRANSITION_PRIORITY::MODE_SAFETY,
						"landed from wallrun",
						"ParkourWallRunAbility"
					);
					return false;
				}

				return true;
			}

			void Stop(MovementContext& context) override {
				if (auto* parkour = AsParkour(context)) {
					parkour->EndWallRun();
				}
			}
		};

		class ParkourSlideAbility final : public IMovementAbility {
		public:
			void Init(ConsoleSystem* console) override {
				mConsole = console;
			}

			[[nodiscard]] MOVEMENT_ABILITY_SLOT GetSlot() const override {
				return MOVEMENT_ABILITY_SLOT::SLIDE;
			}

			[[nodiscard]] std::string_view GetDebugName() const override {
				return "parkour_slide";
			}

			[[nodiscard]] bool IsForcedAbility() const override {
				return false;
			}

			[[nodiscard]] bool
			CanRunInMode(MOVEMENT_MODE_ID modeId) const override {
				return modeId == MOVEMENT_MODE_ID::GROUND;
			}

			[[nodiscard]] bool CanStart(
				const MovementContext& context
			) const override {
				const auto* parkour = dynamic_cast<const
					ParkourMovementComponent*>(context.movementComponent);
				if (!parkour || !context.input.crouchPressed || context.
				    modeState.currentMode != MOVEMENT_MODE_ID::GROUND) {
					return false;
				}
				return parkour->ShouldSlideFromSpeed(
					parkour->GetHorizontalSpeedHu(context.velocity)
				);
			}

			bool Start(MovementContext& context, float) override {
				auto* parkour = AsParkour(context);
				if (!parkour) {
					return false;
				}
				auto& runtime        = parkour->GetParkourRuntime();
				runtime.slide.active = true;
				runtime.slide.time   = 0.0f;
				Vec3 velHorz         = context.velocity;
				velHorz.y            = 0.0f;
				if (velHorz.IsZero()) {
					velHorz = HorizontalNormalized(context.input.forward);
				}
				if (!velHorz.IsZero()) {
					velHorz.Normalize();
					runtime.slide.direction = velHorz;
				} else {
					runtime.slide.direction = Vec3::forward;
				}
				(void)parkour->SetDuckHullEnabled(context, true);
				Vec3 horizontalVelocity = context.velocity;
				horizontalVelocity.y = 0.0f;
				float boostedSpeed = horizontalVelocity.Length() + Math::HtoM(
					                     mConsole ?
						                     mConsole->GetConVarValueOr(
							                     "park_slide_boostspeed", 50.0f
						                     ) :
						                     50.0f
				                     );
				const float speedCap = Math::HtoM(
					mConsole ?
						mConsole->GetConVarValueOr(
							"park_slide_hopspeedcap", 5000.0f
						) :
						5000.0f
				);
				boostedSpeed = std::min(
					boostedSpeed,
					speedCap > 0.0f ?
						speedCap :
						std::numeric_limits<float>::max()
				);
				const float originalY = context.velocity.y;
				context.velocity = runtime.slide.direction * boostedSpeed;
				context.velocity.y = originalY;
				context.modeTickSuppressed = true;
				return true;
			}

			bool Tick(MovementContext& context, float deltaTime) override {
				auto* parkour = AsParkour(context);
				if (!parkour) {
					return false;
				}
				auto& runtime = parkour->GetParkourRuntime();
				if (!runtime.slide.active) {
					return false;
				}
				context.modeTickSuppressed = true;
				(void)parkour->SetDuckHullEnabled(context, true);

				context.isGrounded = false;
				context.velocity.y = 0.0f;

				Vec3 wishDir = GetWishDirHoriz(context);
				ApplyGroundFriction(
					context.velocity,
					mConsole ?
						mConsole->GetConVarValueOr("park_duckfriction", 0.75f) :
						0.75f,
					deltaTime,
					mConsole
				);
				GroundAccelerate(
					context.velocity,
					wishDir,
					mConsole ?
						mConsole->GetConVarValueOr("sv_duckspeed", 63.3f) :
						63.3f,
					mConsole ?
						mConsole->GetConVarValueOr(
							"park_duckaccelerate", 4.0f
						) :
						4.0f,
					deltaTime,
					mConsole
				);

				if (context.input.jumpPressed) {
					runtime.slide.active = false;
					ExecuteGroundJumpAndSwitchToAir(
						context, mConsole, deltaTime
					);
					return false;
				}

				Vec3 movePos = context.transform ?
					               context.transform->GetPosition() :
					               Vec3::zero;
				Vec3 moveVel = context.velocity;
				context.resolver->StepMove(
					movePos,
					moveVel,
					mConsole ?
						mConsole->GetConVarValueOr("sv_stepheight", 18.0f) :
						18.0f,
					deltaTime
				);
				if (context.transform) {
					context.transform->SetPosition(movePos);
				}
				context.velocity = moveVel;

				Physics::Hit groundHit = {};
				const bool   grounded  = context.resolver->ProbeGround(
					                         movePos,
					                         Math::HtoM(2.0f),
					                         &groundHit
				                         ) &&
				                         (groundHit.startSolid || groundHit.
				                          allsolid ||
				                          groundHit.normal.y > 0.7f);
				if (!grounded) {
					runtime.slide.active          = false;
					context.supportEntityGuid     = 0;
					context.supportLinearVelocity = Vec3::zero;
					context.supportStepDelta      = Vec3::zero;
					context.SubmitTransition(
						MOVEMENT_MODE_ID::AIR,
						MOVEMENT_TRANSITION_PRIORITY::MODE_SAFETY,
						"ground lost while sliding",
						"ParkourSlideAbility"
					);
					return false;
				}

				context.isGrounded        = true;
				context.supportEntityGuid = groundHit.hitEntityGuid;
				context.velocity.y        = 0.0f;
				const Vec3 groundNormal   = groundHit.normal;
				if (groundNormal.y < 0.999f && groundNormal.y > 0.0f) {
					const float gravityM = Math::HtoM(
						mConsole ?
							mConsole->GetConVarValueOr("sv_gravity", 800.0f) :
							800.0f
					);
					const Vec3  gravityVec = Vec3::down * gravityM;
					const float dotGN      = gravityVec.Dot(groundNormal);
					const Vec3  slopeForce =
						(gravityVec - groundNormal * dotGN) *
						(mConsole ?
							 mConsole->GetConVarValueOr(
								 "park_slide_gravityscale", 1.75f
							 ) :
							 1.75f);
					context.velocity += slopeForce * deltaTime;
				}

				runtime.slide.time       += std::max(0.0f, deltaTime);
				const float maxSlideTime = mConsole ?
					                           mConsole->GetConVarValueOr(
						                           "park_slide_maxtime", 20.0f
					                           ) :
					                           20.0f;
				if (runtime.slide.time >= maxSlideTime) {
					runtime.slide.active = false;
					return false;
				}

				const float speedHu = parkour->GetHorizontalSpeedHu(
					context.velocity
				);
				const float stopSpeedHu = mConsole ?
					                          mConsole->GetConVarValueOr(
						                          "park_slide_stopspeed", 50.0f
					                          ) :
					                          50.0f;
				if (speedHu < stopSpeedHu) {
					runtime.slide.active = false;
					return false;
				}

				if (!context.input.crouchPressed) {
					runtime.slide.active = false;
					return false;
				}

				return true;
			}

			void Stop(MovementContext& context) override {
				if (auto* parkour = AsParkour(context)) {
					parkour->GetParkourRuntime().slide.active = false;
				}
			}
		};

		class ParkourSpeedVaultAbility final : public IMovementAbility {
		public:
			void Init(ConsoleSystem* console) override {
				mConsole = console;
			}

			[[nodiscard]] MOVEMENT_ABILITY_SLOT GetSlot() const override {
				return MOVEMENT_ABILITY_SLOT::SPEED_VAULT;
			}

			[[nodiscard]] std::string_view GetDebugName() const override {
				return "parkour_speedvault";
			}

			[[nodiscard]] bool IsForcedAbility() const override {
				return true;
			}

			[[nodiscard]] bool CanRunInMode(MOVEMENT_MODE_ID) const override {
				return true;
			}

			[[nodiscard]] bool CanStart(
				const MovementContext& context
			) const override {
				const auto* parkour = dynamic_cast<const
					ParkourMovementComponent*>(context.movementComponent);
				if (!parkour || !context.resolver || !context.transform) {
					return false;
				}
				const auto& runtime = parkour->GetParkourRuntime();
				if (runtime.vault.active || runtime.vault.cooldown > 0.0f ||
				    context.isGrounded) {
					return false;
				}
				if (context.input.crouchPressed || context.input.moveAxis.z <=
				    0.5f) {
					return false;
				}
				if (parkour->IsDuckHullEnabled() && !parkour->CanStandAt(
					    context
				    )) {
					return false;
				}
				SpeedVaultTrajectory trajectory = {};
				return TryBuildSpeedVaultTrajectory(
					context, mConsole, trajectory
				);
			}

			bool Start(MovementContext& context, float) override {
				auto* parkour = AsParkour(context);
				if (!parkour) {
					return false;
				}
				if (!parkour->SetDuckHullEnabled(context, false)) {
					return false;
				}

				SpeedVaultTrajectory trajectory = {};
				if (!TryBuildSpeedVaultTrajectory(
					context, mConsole, trajectory
				)) {
					return false;
				}

				auto& runtime                 = parkour->GetParkourRuntime();
				runtime.vault.active          = true;
				runtime.vault.time            = 0.0f;
				runtime.vault.startPos        = trajectory.startPos;
				runtime.vault.apexPos         = trajectory.apexPos;
				runtime.vault.endPos          = trajectory.endPos;
				runtime.vault.preVelocity     = trajectory.preVelocity;
				context.velocity              = Vec3::zero;
				context.isGrounded            = false;
				context.supportEntityGuid     = 0;
				context.supportLinearVelocity = Vec3::zero;
				context.supportStepDelta      = Vec3::zero;
				context.modeTickSuppressed    = true;
				context.SubmitTransition(
					MOVEMENT_MODE_ID::AIR,
					MOVEMENT_TRANSITION_PRIORITY::FORCED_ABILITY, "vault start",
					"ParkourSpeedVaultAbility"
				);
				return true;
			}

			bool Tick(MovementContext& context, float deltaTime) override {
				auto* parkour = AsParkour(context);
				if (!parkour || !context.transform) {
					return false;
				}
				auto& runtime = parkour->GetParkourRuntime();
				if (!runtime.vault.active) {
					return false;
				}
				context.modeTickSuppressed = true;
				const float duration       = std::max(
					1.0e-4f,
					mConsole ?
						mConsole->GetConVarValueOr(
							"park_vault_duration", 0.25f
						) :
						0.25f
				);
				runtime.vault.time += std::max(0.0f, deltaTime);
				const float t      = std::clamp(
					runtime.vault.time / duration, 0.0f, 1.0f
				);
				const float u = 1.0f - t;
				context.transform->SetPosition(
					runtime.vault.startPos * (u * u) + runtime.vault.apexPos * (
						2.0f * u * t) + runtime.vault.endPos * (t * t)
				);
				parkour->SyncCollisionHull(context.transform);
				context.velocity              = Vec3::zero;
				context.isGrounded            = false;
				context.supportEntityGuid     = 0;
				context.supportLinearVelocity = Vec3::zero;
				context.supportStepDelta      = Vec3::zero;
				if (t >= 1.0f) {
					runtime.vault.active   = false;
					runtime.vault.cooldown = mConsole ?
						                         mConsole->GetConVarValueOr(
							                         "park_vault_cooldown", 0.3f
						                         ) :
						                         0.3f;
					Vec3 horizontalVel          = runtime.vault.preVelocity;
					horizontalVel.y             = 0.0f;
					float       horizontalSpeed = horizontalVel.Length();
					const float minExitSpeed    = Math::HtoM(
						mConsole ?
							mConsole->GetConVarValueOr(
								"park_vault_minspeed", 150.0f
							) :
							150.0f
					);
					horizontalSpeed = std::max(horizontalSpeed, minExitSpeed);

					Vec3 vaultDir =
						runtime.vault.endPos - runtime.vault.startPos;
					vaultDir.y = 0.0f;
					if (!vaultDir.IsZero()) {
						vaultDir.Normalize();
					} else {
						vaultDir = HorizontalNormalized(context.input.forward);
						if (vaultDir.IsZero()) {
							vaultDir = Vec3::forward;
						}
					}

					context.velocity      = vaultDir * horizontalSpeed;
					context.velocity.y    = 0.0f;
					context.isGrounded    = true;
					runtime.hasDoubleJump = true;
					context.SubmitTransition(
						MOVEMENT_MODE_ID::GROUND,
						MOVEMENT_TRANSITION_PRIORITY::FORCED_ABILITY,
						"vault end", "ParkourSpeedVaultAbility"
					);
					return true;
				}
				return true;
			}

			void Stop(MovementContext& context) override {
				if (auto* parkour = AsParkour(context)) {
					parkour->GetParkourRuntime().vault.active = false;
				}
			}
		};

		class ParkourGrappleAbility final : public IMovementAbility {
		public:
			void Init(ConsoleSystem* console) override {
				mConsole = console;
			}

			[[nodiscard]] MOVEMENT_ABILITY_SLOT GetSlot() const override {
				return MOVEMENT_ABILITY_SLOT::GRAPPLE;
			}

			[[nodiscard]] std::string_view GetDebugName() const override {
				return "parkour_grapple";
			}

			[[nodiscard]] bool IsForcedAbility() const override {
				return false;
			}

			[[nodiscard]] bool CanRunInMode(MOVEMENT_MODE_ID) const override {
				return true;
			}

			[[nodiscard]] bool CanStart(
				const MovementContext& context
			) const override {
				const auto* parkour = dynamic_cast<const
					ParkourMovementComponent*>(context.movementComponent);
				return parkour && parkour->GetActionFrameInput().weapon.
				                           secondary.pressed;
			}

			bool Start(MovementContext& context, float) override {
				if (auto* parkour = AsParkour(context)) {
					parkour->GetParkourRuntime().grapple.isActive  = true;
					parkour->GetParkourRuntime().grapple.isLatched = false;
					return true;
				}
				return false;
			}

			bool Tick(MovementContext& context, float) override {
				auto* parkour = AsParkour(context);
				if (!parkour) {
					return false;
				}
				if (!parkour->GetActionFrameInput().weapon.secondary.held) {
					parkour->GetParkourRuntime().grapple.Reset();
					return false;
				}
				return true;
			}

			void Stop(MovementContext& context) override {
				if (auto* parkour = AsParkour(context)) {
					parkour->GetParkourRuntime().grapple.Reset();
				}
			}
		};
	}

	void RegisterParkourMovementAbilities(
		GameMovementStateMachine& stateMachine
	) {
		stateMachine.AddAbility(std::make_shared<JumpMovementAbility>());
		stateMachine.AddAbility(std::make_shared<ParkourCrouchAbility>());
		stateMachine.AddAbility(std::make_shared<ParkourBlinkAbility>());
		stateMachine.AddAbility(std::make_shared<ParkourSpeedVaultAbility>());
		stateMachine.AddAbility(std::make_shared<ParkourWallRunAbility>());
		stateMachine.AddAbility(std::make_shared<ParkourDoubleJumpAbility>());
		stateMachine.AddAbility(std::make_shared<ParkourSlideAbility>());
		stateMachine.AddAbility(std::make_shared<ParkourGrappleAbility>());
	}
}
