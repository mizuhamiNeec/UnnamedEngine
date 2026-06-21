#include "SequenceRuntime.h"

#include <algorithm>
#include <limits>

#include "core/assets/AssetManager.h"
#include "core/assets/types/SequenceAssetData.h"

#include "engine/scene/Scene.h"
#include "engine/sequence/SequencePlayer.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/mesh/SkeletalAnimationComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/world/GameplayCueBus.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] float EvaluateRichCurve(
			const SequenceRichCurveAssetData& curve,
			const float                       frame,
			const float                       fallback = 0.0f
		) {
			if (curve.keys.empty()) {
				return fallback;
			}
			if (curve.keys.size() == 1) {
				return curve.keys.front().value;
			}

			if (frame <= static_cast<float>(curve.keys.front().frame)) {
				return curve.keys.front().value;
			}
			if (frame >= static_cast<float>(curve.keys.back().frame)) {
				return curve.keys.back().value;
			}

			for (size_t i = 0; i + 1 < curve.keys.size(); ++i) {
				const SequenceFloatKeyAssetData& lhs = curve.keys[i];
				const SequenceFloatKeyAssetData& rhs = curve.keys[i + 1];
				const float lhsFrame = static_cast<float>(lhs.frame);
				const float rhsFrame = static_cast<float>(rhs.frame);
				if (frame < lhsFrame || frame > rhsFrame) {
					continue;
				}

				const float segmentFrames = std::max(1.0f, rhsFrame - lhsFrame);
				const float t             = std::clamp(
					(frame - lhsFrame) / segmentFrames, 0.0f, 1.0f
				);

				switch (lhs.interpolation) {
					case SEQUENCE_INTERPOLATION_MODE::MODE_STEP: return lhs.
							value;
					case SEQUENCE_INTERPOLATION_MODE::MODE_LINEAR: return
							lhs.value + (rhs.value - lhs.value) * t;
					case SEQUENCE_INTERPOLATION_MODE::MODE_CUBIC: {
						// Hermite曲線で接線付き補間します。
						const float t2  = t * t;
						const float t3  = t2 * t;
						const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
						const float h10 = t3 - 2.0f * t2 + t;
						const float h01 = -2.0f * t3 + 3.0f * t2;
						const float h11 = t3 - t2;
						return
							h00 * lhs.value +
							h10 * lhs.leaveTangent * segmentFrames +
							h01 * rhs.value +
							h11 * rhs.arriveTangent * segmentFrames;
					}
				}
			}

			return curve.keys.back().value;
		}

		[[nodiscard]] bool EvaluateBoolKeys(
			const std::vector<SequenceBoolKeyAssetData>& keys,
			const float                                  frame,
			const bool                                   fallback = false
		) {
			if (keys.empty()) {
				return fallback;
			}

			bool value = keys.front().value;
			for (const SequenceBoolKeyAssetData& key : keys) {
				if (frame < static_cast<float>(key.frame)) {
					break;
				}
				value = key.value;
			}
			return value;
		}

		[[nodiscard]] uint64_t EvaluateCameraCut(
			const std::vector<SequenceCameraCutKeyAssetData>& keys,
			const float                                       frame
		) {
			if (keys.empty()) {
				return 0;
			}

			uint64_t cameraGuid = keys.front().cameraEntityGuid;
			for (const SequenceCameraCutKeyAssetData& key : keys) {
				if (frame < static_cast<float>(key.frame)) {
					break;
				}
				cameraGuid = key.cameraEntityGuid;
			}
			return cameraGuid;
		}

		[[nodiscard]] bool IsSectionActive(
			const SequenceSectionAssetData& section,
			const float                     frame
		) {
			return
				frame >= static_cast<float>(section.startFrame) &&
				frame <= static_cast<float>(section.endFrame);
		}

		[[nodiscard]] bool IsFrameInTraversal(
			const SequenceTraversalRange& traversal,
			const float                   frame
		) {
			if (!traversal.valid) {
				return false;
			}

			if (traversal.direction == SEQUENCE_PLAYBACK_DIRECTION::FORWARD) {
				if (!traversal.wrapped) {
					return frame > traversal.startFrame && frame <= traversal.
					       endFrame;
				}
				return frame > traversal.startFrame || frame <= traversal.
				       endFrame;
			}

			if (!traversal.wrapped) {
				return frame >= traversal.endFrame && frame < traversal.
				       startFrame;
			}
			return frame >= traversal.endFrame || frame < traversal.startFrame;
		}

		void SortByPriorityAndOrder(auto& contributions) {
			std::ranges::sort(
				contributions,
				[](const auto& lhs, const auto& rhs) {
					if (lhs.priority == rhs.priority) {
						return lhs.order < rhs.order;
					}
					return lhs.priority < rhs.priority;
				}
			);
		}

		[[nodiscard]] uint64_t BuildEventDispatchGuardKey(
			const uint64_t playerId,
			const uint64_t keyId,
			const uint64_t traversalSerial
		) {
			uint64_t hash = 0xcbf29ce484222325ull;
			hash          ^= playerId + 0x9e3779b97f4a7c15ull + (hash << 6) + (
				hash >> 2);
			hash ^= keyId + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
			hash ^= traversalSerial + 0x9e3779b97f4a7c15ull + (hash << 6) + (
				hash >> 2);
			return hash;
		}
	}

	SequenceRuntime::SequenceRuntime(World* world) : mWorld(world) {
	}

	void SequenceRuntime::SetWorld(World* world) {
		mWorld = world;
	}

	void SequenceRuntime::RegisterPlayer(
		const std::shared_ptr<SequencePlayer>& player
	) {
		if (!player) {
			return;
		}
		mPlayers[player->GetPlayerId()] = player;
	}

	void SequenceRuntime::UnregisterPlayer(const uint64_t playerId) {
		if (playerId == 0 || mPlayers.empty()) {
			return;
		}
		if (const auto it = mPlayers.find(playerId);
			it != mPlayers.end()) {
			mPlayers.erase(it);
		}
	}

	void SequenceRuntime::AdvanceAndApplyPreSimulation(
		const float deltaSeconds
	) {
		mTouchedFloatKeys.clear();
		mTouchedBoolKeys.clear();
		mTouchedVec3Keys.clear();
		mTouchedTransforms.clear();
		mTouchedVisibility.clear();
		mTouchedActivation.clear();
		mTouchedCamera            = false;
		mContributionOrderCounter = 0;

		AdvancePlayers(deltaSeconds);
		EvaluatePreSimulation();
	}

	void SequenceRuntime::ApplyPostSimulation() {
		EvaluatePostSimulation();
		RestoreUntouchedState();
	}

	void SequenceRuntime::EditorTick(const float deltaSeconds) {
		AdvanceAndApplyPreSimulation(deltaSeconds);
		ApplyPostSimulation();
	}

	void SequenceRuntime::Clear() {
		mPlayers.clear();
		mPreAnimatedState.Clear();
		mFloatTargetMeta.clear();
		mBoolTargetMeta.clear();
		mVec3TargetMeta.clear();
		mFloatContributions.clear();
		mBoolContributions.clear();
		mVec3Contributions.clear();
		mTransformContributions.clear();
		mVisibilityContributions.clear();
		mActivationContributions.clear();
		mCameraContributions.clear();
		mSkeletalContributions.clear();
		mEventDispatches.clear();
	}

	Entity* SequenceRuntime::ResolveEntity(const uint64_t entityGuid) const {
		if (!mWorld || entityGuid == 0) {
			return nullptr;
		}
		Scene* scene = mWorld->GetScenePtr();
		return scene ? scene->FindEntity(entityGuid) : nullptr;
	}

	const SequenceBindingAssetData* SequenceRuntime::FindBinding(
		const SequenceAssetData& asset,
		const uint64_t           bindingId
	) const {
		if (bindingId == 0) {
			return nullptr;
		}
		for (const SequenceBindingAssetData& binding : asset.bindings) {
			if (binding.bindingId == bindingId) {
				return &binding;
			}
		}
		return nullptr;
	}

	void SequenceRuntime::AdvancePlayers(const float deltaSeconds) {
		PruneExpiredPlayers();
		if (!mWorld) {
			return;
		}
		const AssetManager* assetManager = mWorld->GetAssetManager();
		if (!assetManager) {
			return;
		}

		for (auto& [playerId, playerWeak] : mPlayers) {
			(void)playerId;
			const std::shared_ptr<SequencePlayer> player = playerWeak.lock();
			if (!player || player->GetAssetId() == kInvalidAssetID) {
				continue;
			}

			const SequenceAssetData* asset = assetManager->Get<
				SequenceAssetData>(
				player->GetAssetId()
			);
			if (!asset) {
				continue;
			}

			const uint64_t version = assetManager->Meta(player->GetAssetId()).
			                                       version;
			if (
				player->GetCompiled() == nullptr ||
				player->GetCompiledVersion() != version
			) {
				player->SetCompiled(
					std::make_shared<SequenceAssetData>(*asset), version
				);
				player->ClampCurrentFrame(static_cast<float>(std::max<int64_t>(
					0,
					asset->lengthFrames
				)));
			}

			const CompiledSequence* compiled = player->GetCompiled();
			if (!compiled) {
				continue;
			}

			player->AdvanceFrame(std::max(0.0f, deltaSeconds), *compiled);
		}
	}

	void SequenceRuntime::EvaluatePreSimulation() {
		mFloatContributions.clear();
		mBoolContributions.clear();
		mVec3Contributions.clear();
		mTransformContributions.clear();
		mSkeletalContributions.clear();

		std::vector<std::shared_ptr<SequencePlayer>> players = {};
		players.reserve(mPlayers.size());
		for (auto& [playerId, playerWeak] : mPlayers) {
			(void)playerId;
			if (auto player = playerWeak.lock()) {
				players.emplace_back(std::move(player));
			}
		}
		std::ranges::sort(
			players,
			[](
			const std::shared_ptr<SequencePlayer>& lhs,
			const std::shared_ptr<SequencePlayer>& rhs
		) {
				return lhs->GetPlayerId() < rhs->GetPlayerId();
			}
		);

		uint64_t playerOrder = 0;
		for (const std::shared_ptr<SequencePlayer>& player : players) {
			if (!player || !player->IsEvaluating()) {
				continue;
			}
			const CompiledSequence* compiled = player->GetCompiled();
			if (!compiled || !compiled->GetAsset()) {
				continue;
			}
			const SequenceAssetData& asset = *compiled->GetAsset();

			for (const CompiledSequenceTrack& track : compiled->GetTracks()) {
				if (!track.source) {
					continue;
				}

				const SequenceBindingAssetData* binding = FindBinding(
					asset,
					track.source->bindingId
				);
				CollectTrackPre(
					*player, asset, *track.source, binding, playerOrder++
				);
			}
		}

		ApplyFloatContributions();
		ApplyBoolContributions();
		ApplyVec3Contributions();
		ApplyTransformContributions();
		ApplySkeletalContributions();
	}

	void SequenceRuntime::EvaluatePostSimulation() {
		mVisibilityContributions.clear();
		mActivationContributions.clear();
		mCameraContributions.clear();
		mEventDispatches.clear();
		mEventDispatchGuard.clear();

		std::vector<std::shared_ptr<SequencePlayer>> players = {};
		players.reserve(mPlayers.size());
		for (auto& [playerId, playerWeak] : mPlayers) {
			(void)playerId;
			if (auto player = playerWeak.lock()) {
				players.emplace_back(std::move(player));
			}
		}
		std::ranges::sort(
			players,
			[](
			const std::shared_ptr<SequencePlayer>& lhs,
			const std::shared_ptr<SequencePlayer>& rhs
		) {
				return lhs->GetPlayerId() < rhs->GetPlayerId();
			}
		);

		uint64_t playerOrder = 0;
		for (const std::shared_ptr<SequencePlayer>& player : players) {
			if (!player || !player->IsEvaluating()) {
				if (player) {
					player->ClearEventSuppression();
				}
				continue;
			}
			const CompiledSequence* compiled = player->GetCompiled();
			if (!compiled || !compiled->GetAsset()) {
				player->ClearEventSuppression();
				continue;
			}
			const SequenceAssetData& asset = *compiled->GetAsset();

			for (const CompiledSequenceTrack& track : compiled->GetTracks()) {
				if (!track.source) {
					continue;
				}
				const SequenceBindingAssetData* binding = FindBinding(
					asset,
					track.source->bindingId
				);
				CollectTrackPost(
					*player, asset, *track.source, binding, playerOrder++
				);
			}
			player->ClearEventSuppression();
		}

		ApplyEntityVisibilityContributions();
		ApplyEntityActivationContributions();
		ApplyCameraContributions();
		DispatchEvents();
	}

	void SequenceRuntime::RestoreUntouchedState() {
		// Float値の復元を行います。
		std::vector<std::string> floatRestoreKeys = {};
		floatRestoreKeys.reserve(mPreAnimatedState.GetSavedFloats().size());
		for (const auto& [key, value] : mPreAnimatedState.GetSavedFloats()) {
			(void)value;
			if (!mTouchedFloatKeys.contains(key)) {
				floatRestoreKeys.emplace_back(key);
			}
		}
		for (const std::string& key : floatRestoreKeys) {
			const auto metaIt = mFloatTargetMeta.find(key);
			if (metaIt == mFloatTargetMeta.end()) {
				mPreAnimatedState.RemoveFloat(key);
				continue;
			}

			Entity* entity = ResolveEntity(metaIt->second.entityGuid);
			const SequenceFloatAccessor* accessor = mAccessorRegistry.FindFloat(
				metaIt->second.componentStableName,
				metaIt->second.propertyPath
			);
			float savedValue = 0.0f;
			if (
				entity &&
				accessor &&
				accessor->set &&
				mPreAnimatedState.TryGetFloat(key, savedValue)
			) {
				(void)accessor->set(*entity, savedValue);
			}
			mPreAnimatedState.RemoveFloat(key);
			mFloatTargetMeta.erase(key);
		}

		// Bool値の復元を行います。
		std::vector<std::string> boolRestoreKeys = {};
		boolRestoreKeys.reserve(mPreAnimatedState.GetSavedBools().size());
		for (const auto& [key, value] : mPreAnimatedState.GetSavedBools()) {
			(void)value;
			if (!mTouchedBoolKeys.contains(key)) {
				boolRestoreKeys.emplace_back(key);
			}
		}
		for (const std::string& key : boolRestoreKeys) {
			const auto metaIt = mBoolTargetMeta.find(key);
			if (metaIt == mBoolTargetMeta.end()) {
				mPreAnimatedState.RemoveBool(key);
				continue;
			}

			Entity* entity = ResolveEntity(metaIt->second.entityGuid);
			const SequenceBoolAccessor* accessor = mAccessorRegistry.FindBool(
				metaIt->second.componentStableName,
				metaIt->second.propertyPath
			);
			bool savedValue = false;
			if (
				entity &&
				accessor &&
				accessor->set &&
				mPreAnimatedState.TryGetBool(key, savedValue)
			) {
				(void)accessor->set(*entity, savedValue);
			}
			mPreAnimatedState.RemoveBool(key);
			mBoolTargetMeta.erase(key);
		}

		// Vec3値の復元を行います。
		std::vector<std::string> vec3RestoreKeys = {};
		vec3RestoreKeys.reserve(mPreAnimatedState.GetSavedVec3().size());
		for (const auto& [key, value] : mPreAnimatedState.GetSavedVec3()) {
			(void)value;
			if (!mTouchedVec3Keys.contains(key)) {
				vec3RestoreKeys.emplace_back(key);
			}
		}
		for (const std::string& key : vec3RestoreKeys) {
			const auto metaIt = mVec3TargetMeta.find(key);
			if (metaIt == mVec3TargetMeta.end()) {
				mPreAnimatedState.RemoveVec3(key);
				continue;
			}

			Entity* entity = ResolveEntity(metaIt->second.entityGuid);
			const SequenceVec3Accessor* accessor = mAccessorRegistry.FindVec3(
				metaIt->second.componentStableName,
				metaIt->second.propertyPath
			);
			Vec3 savedValue = Vec3::zero;
			if (
				entity &&
				accessor &&
				accessor->set &&
				mPreAnimatedState.TryGetVec3(key, savedValue)
			) {
				(void)accessor->set(*entity, savedValue);
			}
			mPreAnimatedState.RemoveVec3(key);
			mVec3TargetMeta.erase(key);
		}

		// Transform値の復元を行います。
		std::vector<uint64_t> transformRestoreKeys = {};
		transformRestoreKeys.reserve(
			mPreAnimatedState.GetSavedTransforms().size()
		);
		for (const auto& [entityGuid, snapshot] : mPreAnimatedState.
		     GetSavedTransforms()) {
			(void)snapshot;
			if (!mTouchedTransforms.contains(entityGuid)) {
				transformRestoreKeys.emplace_back(entityGuid);
			}
		}
		for (const uint64_t entityGuid : transformRestoreKeys) {
			Entity* entity    = ResolveEntity(entityGuid);
			auto*   transform = entity ?
				                  entity->GetComponent<TransformComponent>() :
				                  nullptr;
			SequenceTransformSnapshot snapshot = {};
			if (
				transform &&
				mPreAnimatedState.TryGetTransform(entityGuid, snapshot)
			) {
				transform->SetPosition(snapshot.position);
				transform->SetRotation(snapshot.rotation);
				transform->SetScale(snapshot.scale);
			}
			mPreAnimatedState.RemoveTransform(entityGuid);
		}

		// Entityの可視状態復元を行います。
		std::vector<uint64_t> visibilityRestoreKeys = {};
		visibilityRestoreKeys.reserve(
			mPreAnimatedState.GetSavedEntityVisible().size()
		);
		for (const auto& [entityGuid, value] : mPreAnimatedState.
		     GetSavedEntityVisible()) {
			(void)value;
			if (!mTouchedVisibility.contains(entityGuid)) {
				visibilityRestoreKeys.emplace_back(entityGuid);
			}
		}
		for (const uint64_t entityGuid : visibilityRestoreKeys) {
			Entity* entity     = ResolveEntity(entityGuid);
			bool    savedValue = true;
			if (entity && mPreAnimatedState.TryGetEntityVisible(
				    entityGuid, savedValue
			    )) {
				entity->SetVisible(savedValue);
			}
			mPreAnimatedState.RemoveEntityVisible(entityGuid);
		}

		// Entityのアクティブ状態復元を行います。
		std::vector<uint64_t> activeRestoreKeys = {};
		activeRestoreKeys.reserve(
			mPreAnimatedState.GetSavedEntityActive().size()
		);
		for (const auto& [entityGuid, value] : mPreAnimatedState.
		     GetSavedEntityActive()) {
			(void)value;
			if (!mTouchedActivation.contains(entityGuid)) {
				activeRestoreKeys.emplace_back(entityGuid);
			}
		}
		for (const uint64_t entityGuid : activeRestoreKeys) {
			Entity* entity     = ResolveEntity(entityGuid);
			bool    savedValue = true;
			if (entity && mPreAnimatedState.TryGetEntityActive(
				    entityGuid, savedValue
			    )) {
				entity->SetActive(savedValue);
			}
			mPreAnimatedState.RemoveEntityActive(entityGuid);
		}

		// Camera状態復元を行います。
		if (!mTouchedCamera) {
			SequenceCameraSnapshot snapshot = {};
			if (mWorld && mPreAnimatedState.TryGetCamera(snapshot)) {
				if (snapshot.hasCurrent && snapshot.currentCameraGuid != 0) {
					if (!mWorld->GetCameraManager().SetCurrentCamera(
						snapshot.currentCameraGuid
					)) {
						mWorld->GetCameraManager().ClearCurrentCamera();
					}
				} else {
					mWorld->GetCameraManager().ClearCurrentCamera();
				}
				mPreAnimatedState.RemoveCamera();
			}
		}
	}

	void SequenceRuntime::PruneExpiredPlayers() {
		for (auto it = mPlayers.begin(); it != mPlayers.end();) {
			if (it->second.expired()) {
				it = mPlayers.erase(it);
			} else {
				++it;
			}
		}
	}

	void SequenceRuntime::CollectTrackPre(
		const SequencePlayer&           player,
		const SequenceAssetData&        asset,
		const SequenceTrackAssetData&   track,
		const SequenceBindingAssetData* binding,
		const uint64_t                  playerOrder
	) {
		(void)asset;
		const float currentFrame = player.GetCurrentFrame();
		const float playerWeight = player.GetWeight();

		const auto PushTransform = [&](
			const SequenceSectionAssetData& section
		) {
			if (!binding || binding->entityGuid == 0) {
				return;
			}

			TransformContribution contribution = {};
			contribution.entityGuid = binding->entityGuid;
			contribution.priority = track.priority + section.priority;
			contribution.blendMode = track.blendMode;
			contribution.order = playerOrder;

			if (!section.transformPosX.keys.empty() ||
			    !section.transformPosY.keys.empty() ||
			    !section.transformPosZ.keys.empty()) {
				contribution.hasPosition = true;
				contribution.position    =
					Vec3(
						EvaluateRichCurve(
							section.transformPosX,
							currentFrame, 0.0f
						),
						EvaluateRichCurve(
							section.transformPosY,
							currentFrame, 0.0f
						),
						EvaluateRichCurve(
							section.transformPosZ,
							currentFrame, 0.0f
						)
					) * playerWeight;
			}
			if (!section.transformRotX.keys.empty() ||
			    !section.transformRotY.keys.empty() ||
			    !section.transformRotZ.keys.empty() ||
			    !section.transformRotW.keys.empty()) {
				contribution.hasRotation = true;
				contribution.rotation    = Quaternion(
					EvaluateRichCurve(
						section.transformRotX, currentFrame, 0.0f
					),
					EvaluateRichCurve(
						section.transformRotY, currentFrame, 0.0f
					),
					EvaluateRichCurve(
						section.transformRotZ, currentFrame, 0.0f
					),
					EvaluateRichCurve(section.transformRotW, currentFrame, 1.0f)
				).Normalized();
			}
			if (!section.transformScaleX.keys.empty() ||
			    !section.transformScaleY.keys.empty() ||
			    !section.transformScaleZ.keys.empty()) {
				contribution.hasScale = true;
				contribution.scale    =
					Vec3(
						EvaluateRichCurve(
							section.transformScaleX,
							currentFrame, 0.0f
						),
						EvaluateRichCurve(
							section.transformScaleY,
							currentFrame, 0.0f
						),
						EvaluateRichCurve(
							section.transformScaleZ,
							currentFrame, 0.0f
						)
					) * playerWeight;
			}
			if (!contribution.hasPosition &&
			    !contribution.hasRotation &&
			    !contribution.hasScale) {
				return;
			}
			mTransformContributions[contribution.entityGuid].emplace_back(
				contribution
			);
		};

		const auto PushPropertyFloat = [&](
			const SequenceSectionAssetData& section
		) {
			if (!binding || binding->entityGuid == 0) {
				return;
			}
			if (section.floatCurve.keys.empty()) {
				return;
			}

			FloatContribution contribution   = {};
			contribution.entityGuid          = binding->entityGuid;
			contribution.componentStableName = binding->componentStableName;
			contribution.propertyPath        = binding->propertyPath;
			contribution.value               = EvaluateRichCurve(
				                     section.floatCurve,
				                     currentFrame
			                     ) * playerWeight;
			contribution.priority  = track.priority + section.priority;
			contribution.blendMode = track.blendMode;
			contribution.order     = playerOrder;

			const std::string key = BuildPropertyKey(
				contribution.entityGuid,
				contribution.componentStableName,
				contribution.propertyPath
			);
			mFloatTargetMeta[key] = FloatTargetMeta{
				.entityGuid          = contribution.entityGuid,
				.componentStableName = contribution.componentStableName,
				.propertyPath        = contribution.propertyPath,
			};
			mFloatContributions[key].emplace_back(std::move(contribution));
		};

		const auto PushPropertyBool = [&](
			const SequenceSectionAssetData& section
		) {
			if (!binding || binding->entityGuid == 0) {
				return;
			}
			if (section.boolKeys.empty()) {
				return;
			}

			BoolContribution contribution    = {};
			contribution.entityGuid          = binding->entityGuid;
			contribution.componentStableName = binding->componentStableName;
			contribution.propertyPath        = binding->propertyPath;
			contribution.value               = EvaluateBoolKeys(
				section.boolKeys, currentFrame
			);
			contribution.priority = track.priority + section.priority;
			contribution.order    = playerOrder;

			const std::string key = BuildPropertyKey(
				contribution.entityGuid,
				contribution.componentStableName,
				contribution.propertyPath
			);
			mBoolTargetMeta[key] = BoolTargetMeta{
				.entityGuid          = contribution.entityGuid,
				.componentStableName = contribution.componentStableName,
				.propertyPath        = contribution.propertyPath,
			};
			mBoolContributions[key].emplace_back(std::move(contribution));
		};

		const auto PushPropertyVec3 = [&](
			const SequenceSectionAssetData& section
		) {
			if (!binding || binding->entityGuid == 0) {
				return;
			}
			if (
				section.vec3XCurve.keys.empty() &&
				section.vec3YCurve.keys.empty() &&
				section.vec3ZCurve.keys.empty()
			) {
				return;
			}

			Vec3Contribution contribution    = {};
			contribution.entityGuid          = binding->entityGuid;
			contribution.componentStableName = binding->componentStableName;
			contribution.propertyPath        = binding->propertyPath;
			contribution.value               = Vec3(
				                     EvaluateRichCurve(
					                     section.vec3XCurve,
					                     currentFrame, 0.0f
				                     ),
				                     EvaluateRichCurve(
					                     section.vec3YCurve,
					                     currentFrame, 0.0f
				                     ),
				                     EvaluateRichCurve(
					                     section.vec3ZCurve,
					                     currentFrame, 0.0f
				                     )
			                     ) * playerWeight;
			contribution.priority  = track.priority + section.priority;
			contribution.blendMode = track.blendMode;
			contribution.order     = playerOrder;

			const std::string key = BuildPropertyKey(
				contribution.entityGuid,
				contribution.componentStableName,
				contribution.propertyPath
			);
			mVec3TargetMeta[key] = Vec3TargetMeta{
				.entityGuid          = contribution.entityGuid,
				.componentStableName = contribution.componentStableName,
				.propertyPath        = contribution.propertyPath,
			};
			mVec3Contributions[key].emplace_back(std::move(contribution));
		};

		const auto PushSkeletal = [&](const SequenceSectionAssetData& section) {
			if (!binding || binding->entityGuid == 0) {
				return;
			}

			SkeletalContribution contribution = {};
			contribution.entityGuid = binding->entityGuid;
			contribution.layerIndex = section.skeletal.layerIndex;
			contribution.priority = track.priority + section.priority;
			contribution.order = playerOrder;

			if (!section.skeletal.weightCurve.keys.empty()) {
				contribution.hasWeight = true;
				contribution.weight    = EvaluateRichCurve(
					section.skeletal.weightCurve,
					currentFrame
				);
			}
			if (!section.skeletal.playbackCurve.keys.empty()) {
				contribution.hasPlayback  = true;
				contribution.playbackTime = EvaluateRichCurve(
					section.skeletal.playbackCurve,
					currentFrame
				);
			}
			if (!section.skeletal.speedCurve.keys.empty()) {
				contribution.hasSpeed = true;
				contribution.speed    = EvaluateRichCurve(
					section.skeletal.speedCurve,
					currentFrame,
					1.0f
				);
			}

			const SequenceTraversalRange traversal = player.
				GetLastTraversalRange();
			if (
				section.skeletal.playOnEnter &&
				!section.skeletal.stateId.empty() &&
				IsFrameInTraversal(traversal,
				                   static_cast<float>(section.startFrame))
			) {
				contribution.playState = true;
				contribution.stateId   = section.skeletal.stateId;
			}
			if (
				section.skeletal.stopOnExit &&
				IsFrameInTraversal(traversal,
				                   static_cast<float>(section.endFrame))
			) {
				contribution.stopLayer = true;
			}

			if (
				!contribution.hasWeight &&
				!contribution.hasPlayback &&
				!contribution.hasSpeed &&
				!contribution.playState &&
				!contribution.stopLayer
			) {
				return;
			}
			mSkeletalContributions.emplace_back(std::move(contribution));
		};

		for (const SequenceSectionAssetData& section : track.sections) {
			if (!IsSectionActive(section, currentFrame)) {
				continue;
			}

			switch (track.trackType) {
				case SEQUENCE_TRACK_TYPE::TRANSFORM: PushTransform(section);
					break;
				case SEQUENCE_TRACK_TYPE::PROPERTY_FLOAT: PushPropertyFloat(
						section
					);
					break;
				case SEQUENCE_TRACK_TYPE::PROPERTY_BOOL: PushPropertyBool(
						section
					);
					break;
				case SEQUENCE_TRACK_TYPE::PROPERTY_VEC3: PushPropertyVec3(
						section
					);
					break;
				case SEQUENCE_TRACK_TYPE::SKELETAL_CONTROL: PushSkeletal(
						section
					);
					break;
				default: break;
			}
		}
	}

	void SequenceRuntime::CollectTrackPost(
		SequencePlayer&                 player,
		const SequenceAssetData&        asset,
		const SequenceTrackAssetData&   track,
		const SequenceBindingAssetData* binding,
		const uint64_t                  playerOrder
	) {
		const float                  currentFrame = player.GetCurrentFrame();
		const SequenceTraversalRange traversal = player.GetLastTraversalRange();

		for (const SequenceSectionAssetData& section : track.sections) {
			switch (track.trackType) {
				case SEQUENCE_TRACK_TYPE::CAMERA_CUT: {
					if (!IsSectionActive(section, currentFrame)) {
						continue;
					}

					const uint64_t cameraGuid = EvaluateCameraCut(
						section.cameraCutKeys,
						currentFrame
					);
					if (cameraGuid == 0) {
						continue;
					}

					mCameraContributions.emplace_back(
						CameraContribution{
							.cameraEntityGuid = cameraGuid,
							.priority = track.priority + section.priority,
							.order = playerOrder,
						}
					);
					break;
				}
				case SEQUENCE_TRACK_TYPE::VISIBILITY: {
					if (!binding || binding->entityGuid == 0) {
						continue;
					}
					if (!IsSectionActive(section, currentFrame) || section.
					    boolKeys.empty()) {
						continue;
					}

					mVisibilityContributions[binding->entityGuid].emplace_back(
						EntityBoolContribution{
							.entityGuid = binding->entityGuid,
							.value      = EvaluateBoolKeys(
								section.boolKeys, currentFrame
							),
							.priority = track.priority + section.priority,
							.order    = playerOrder,
						}
					);
					break;
				}
				case SEQUENCE_TRACK_TYPE::ACTIVATION: {
					if (!binding || binding->entityGuid == 0) {
						continue;
					}
					if (!IsSectionActive(section, currentFrame) || section.
					    boolKeys.empty()) {
						continue;
					}

					mActivationContributions[binding->entityGuid].emplace_back(
						EntityBoolContribution{
							.entityGuid = binding->entityGuid,
							.value      = EvaluateBoolKeys(
								section.boolKeys, currentFrame
							),
							.priority = track.priority + section.priority,
							.order    = playerOrder,
						}
					);
					break;
				}
				case SEQUENCE_TRACK_TYPE::EVENT: {
					if (section.eventKeys.empty()) {
						continue;
					}
					EvaluateEventRange(
						traversal,
						player,
						binding,
						section,
						std::max<int64_t>(0, asset.lengthFrames)
					);
					break;
				}
				default: break;
			}
		}
	}

	void SequenceRuntime::ApplyFloatContributions() {
		for (auto& [targetKey, contributions] : mFloatContributions) {
			const auto metaIt = mFloatTargetMeta.find(targetKey);
			if (metaIt == mFloatTargetMeta.end()) {
				continue;
			}

			Entity* entity = ResolveEntity(metaIt->second.entityGuid);
			if (!entity) {
				continue;
			}

			const SequenceFloatAccessor* accessor = mAccessorRegistry.FindFloat(
				metaIt->second.componentStableName,
				metaIt->second.propertyPath
			);
			if (!accessor || !accessor->get || !accessor->set) {
				continue;
			}

			float baseValue = 0.0f;
			if (!mPreAnimatedState.TryGetFloat(targetKey, baseValue)) {
				if (!accessor->get(*entity, baseValue)) {
					continue;
				}
				mPreAnimatedState.SaveFloatIfMissing(targetKey, baseValue);
			}

			float    absValue      = baseValue;
			float    additiveValue = 0.0f;
			bool     hasAbsolute   = false;
			int32_t  absPriority   = std::numeric_limits<int32_t>::min();
			uint64_t absOrder      = 0;
			for (const FloatContribution& contribution : contributions) {
				if (contribution.blendMode ==
				    SEQUENCE_BLEND_MODE::MODE_ADDITIVE) {
					additiveValue += contribution.value;
					continue;
				}

				if (
					!hasAbsolute ||
					contribution.priority > absPriority ||
					(contribution.priority == absPriority &&
					 contribution.order >= absOrder)
				) {
					hasAbsolute = true;
					absPriority = contribution.priority;
					absOrder    = contribution.order;
					absValue    = contribution.value;
				}
			}

			const float finalValue =
				(hasAbsolute ? absValue : baseValue) + additiveValue;
			if (accessor->set(*entity, finalValue)) {
				mTouchedFloatKeys.emplace(targetKey);
			}
		}
	}

	void SequenceRuntime::ApplyBoolContributions() {
		for (auto& [targetKey, contributions] : mBoolContributions) {
			const auto metaIt = mBoolTargetMeta.find(targetKey);
			if (metaIt == mBoolTargetMeta.end()) {
				continue;
			}

			Entity* entity = ResolveEntity(metaIt->second.entityGuid);
			if (!entity) {
				continue;
			}

			const SequenceBoolAccessor* accessor = mAccessorRegistry.FindBool(
				metaIt->second.componentStableName,
				metaIt->second.propertyPath
			);
			if (!accessor || !accessor->get || !accessor->set) {
				continue;
			}

			bool baseValue = false;
			if (!mPreAnimatedState.TryGetBool(targetKey, baseValue)) {
				if (!accessor->get(*entity, baseValue)) {
					continue;
				}
				mPreAnimatedState.SaveBoolIfMissing(targetKey, baseValue);
			}

			bool     finalValue   = baseValue;
			int32_t  bestPriority = std::numeric_limits<int32_t>::min();
			uint64_t bestOrder    = 0;
			for (const BoolContribution& contribution : contributions) {
				if (
					contribution.priority > bestPriority ||
					(contribution.priority == bestPriority && contribution.order
					 >= bestOrder)
				) {
					bestPriority = contribution.priority;
					bestOrder    = contribution.order;
					finalValue   = contribution.value;
				}
			}

			if (accessor->set(*entity, finalValue)) {
				mTouchedBoolKeys.emplace(targetKey);
			}
		}
	}

	void SequenceRuntime::ApplyVec3Contributions() {
		for (auto& [targetKey, contributions] : mVec3Contributions) {
			const auto metaIt = mVec3TargetMeta.find(targetKey);
			if (metaIt == mVec3TargetMeta.end()) {
				continue;
			}

			Entity* entity = ResolveEntity(metaIt->second.entityGuid);
			if (!entity) {
				continue;
			}

			const SequenceVec3Accessor* accessor = mAccessorRegistry.FindVec3(
				metaIt->second.componentStableName,
				metaIt->second.propertyPath
			);
			if (!accessor || !accessor->get || !accessor->set) {
				continue;
			}

			Vec3 baseValue = Vec3::zero;
			if (!mPreAnimatedState.TryGetVec3(targetKey, baseValue)) {
				if (!accessor->get(*entity, baseValue)) {
					continue;
				}
				mPreAnimatedState.SaveVec3IfMissing(targetKey, baseValue);
			}

			Vec3     absValue      = baseValue;
			Vec3     additiveValue = Vec3::zero;
			bool     hasAbsolute   = false;
			int32_t  absPriority   = std::numeric_limits<int32_t>::min();
			uint64_t absOrder      = 0;
			for (const Vec3Contribution& contribution : contributions) {
				if (contribution.blendMode ==
				    SEQUENCE_BLEND_MODE::MODE_ADDITIVE) {
					additiveValue += contribution.value;
					continue;
				}

				if (
					!hasAbsolute ||
					contribution.priority > absPriority ||
					(contribution.priority == absPriority &&
					 contribution.order >= absOrder)
				) {
					hasAbsolute = true;
					absPriority = contribution.priority;
					absOrder    = contribution.order;
					absValue    = contribution.value;
				}
			}

			const Vec3 finalValue =
				(hasAbsolute ? absValue : baseValue) + additiveValue;
			if (accessor->set(*entity, finalValue)) {
				mTouchedVec3Keys.emplace(targetKey);
			}
		}
	}

	void SequenceRuntime::ApplyTransformContributions() {
		for (auto& [entityGuid, contributions] : mTransformContributions) {
			Entity* entity = ResolveEntity(entityGuid);
			if (!entity) {
				continue;
			}
			auto* transform = entity->GetComponent<TransformComponent>();
			if (!transform) {
				continue;
			}

			SequenceTransformSnapshot snapshot = {};
			if (!mPreAnimatedState.TryGetTransform(entityGuid, snapshot)) {
				snapshot.position = transform->GetPosition();
				snapshot.rotation = transform->GetRotation();
				snapshot.scale    = transform->GetScale();
				mPreAnimatedState.SaveTransformIfMissing(entityGuid, snapshot);
			}

			Quaternion finalRotation = snapshot.rotation;
			bool       applyPosition = false;
			bool       applyScale    = false;
			bool       applyRotation = false;

			Vec3     absPosition         = snapshot.position;
			Vec3     addPosition         = Vec3::zero;
			bool     hasAbsPosition      = false;
			int32_t  absPositionPriority = std::numeric_limits<int32_t>::min();
			uint64_t absPositionOrder    = 0;

			Vec3     absScale         = snapshot.scale;
			Vec3     addScale         = Vec3::zero;
			bool     hasAbsScale      = false;
			int32_t  absScalePriority = std::numeric_limits<int32_t>::min();
			uint64_t absScaleOrder    = 0;

			int32_t  rotPriority = std::numeric_limits<int32_t>::min();
			uint64_t rotOrder    = 0;

			for (const TransformContribution& contribution : contributions) {
				if (contribution.hasPosition) {
					applyPosition = true;
					if (contribution.blendMode ==
					    SEQUENCE_BLEND_MODE::MODE_ADDITIVE) {
						addPosition += contribution.position;
					} else if (
						!hasAbsPosition ||
						contribution.priority > absPositionPriority ||
						(contribution.priority == absPositionPriority &&
						 contribution.order >= absPositionOrder)
					) {
						hasAbsPosition      = true;
						absPositionPriority = contribution.priority;
						absPositionOrder    = contribution.order;
						absPosition         = contribution.position;
					}
				}

				if (contribution.hasScale) {
					applyScale = true;
					if (contribution.blendMode ==
					    SEQUENCE_BLEND_MODE::MODE_ADDITIVE) {
						addScale += contribution.scale;
					} else if (
						!hasAbsScale ||
						contribution.priority > absScalePriority ||
						(contribution.priority == absScalePriority &&
						 contribution.order >= absScaleOrder)
					) {
						hasAbsScale      = true;
						absScalePriority = contribution.priority;
						absScaleOrder    = contribution.order;
						absScale         = contribution.scale;
					}
				}

				if (
					contribution.hasRotation &&
					(contribution.priority > rotPriority ||
					 (contribution.priority == rotPriority &&
					  contribution.order >= rotOrder))
				) {
					applyRotation = true;
					rotPriority   = contribution.priority;
					rotOrder      = contribution.order;
					finalRotation = contribution.rotation;
				}
			}

			if (applyPosition) {
				const Vec3 finalPosition = (hasAbsPosition ?
					                            absPosition :
					                            snapshot.position) +
				                           addPosition;
				transform->SetPosition(finalPosition);
			}
			if (applyScale) {
				const Vec3 finalScale =
					(hasAbsScale ? absScale : snapshot.scale) +
					addScale;
				transform->SetScale(finalScale);
			}
			if (applyRotation) {
				transform->SetRotation(finalRotation);
			}
			if (applyPosition || applyScale || applyRotation) {
				mTouchedTransforms.emplace(entityGuid);
			}
		}
	}

	void SequenceRuntime::ApplyEntityVisibilityContributions() {
		for (auto& [entityGuid, contributions] : mVisibilityContributions) {
			Entity* entity = ResolveEntity(entityGuid);
			if (!entity) {
				continue;
			}

			bool baseValue = entity->IsVisible();
			if (!mPreAnimatedState.TryGetEntityVisible(entityGuid, baseValue)) {
				mPreAnimatedState.SaveEntityVisibleIfMissing(
					entityGuid, baseValue
				);
			}

			bool     finalValue   = baseValue;
			int32_t  bestPriority = std::numeric_limits<int32_t>::min();
			uint64_t bestOrder    = 0;
			for (const EntityBoolContribution& contribution : contributions) {
				if (
					contribution.priority > bestPriority ||
					(contribution.priority == bestPriority &&
					 contribution.order >= bestOrder)
				) {
					bestPriority = contribution.priority;
					bestOrder    = contribution.order;
					finalValue   = contribution.value;
				}
			}
			entity->SetVisible(finalValue);
			mTouchedVisibility.emplace(entityGuid);
		}
	}

	void SequenceRuntime::ApplyEntityActivationContributions() {
		for (auto& [entityGuid, contributions] : mActivationContributions) {
			Entity* entity = ResolveEntity(entityGuid);
			if (!entity) {
				continue;
			}

			bool baseValue = entity->IsActive();
			if (!mPreAnimatedState.TryGetEntityActive(entityGuid, baseValue)) {
				mPreAnimatedState.SaveEntityActiveIfMissing(
					entityGuid, baseValue
				);
			}

			bool     finalValue   = baseValue;
			int32_t  bestPriority = std::numeric_limits<int32_t>::min();
			uint64_t bestOrder    = 0;
			for (const EntityBoolContribution& contribution : contributions) {
				if (
					contribution.priority > bestPriority ||
					(contribution.priority == bestPriority &&
					 contribution.order >= bestOrder)
				) {
					bestPriority = contribution.priority;
					bestOrder    = contribution.order;
					finalValue   = contribution.value;
				}
			}
			entity->SetActive(finalValue);
			mTouchedActivation.emplace(entityGuid);
		}
	}

	void SequenceRuntime::ApplyCameraContributions() {
		if (mCameraContributions.empty() || !mWorld) {
			return;
		}

		SortByPriorityAndOrder(mCameraContributions);
		const CameraContribution& winner = mCameraContributions.back();

		SequenceCameraSnapshot snapshot = {};
		if (!mPreAnimatedState.TryGetCamera(snapshot)) {
			const WorldCameraManager& cameraManager = mWorld->
				GetCameraManager();
			snapshot.hasCurrent        = cameraManager.HasCurrentCamera();
			snapshot.currentCameraGuid = cameraManager.GetCurrentCameraGuid();
			mPreAnimatedState.SaveCameraIfMissing(snapshot);
		}

		(void)mWorld->GetCameraManager().SetCurrentCamera(
			winner.cameraEntityGuid
		);
		mTouchedCamera = true;
	}

	void SequenceRuntime::ApplySkeletalContributions() {
		SortByPriorityAndOrder(mSkeletalContributions);
		for (const SkeletalContribution& contribution :
		     mSkeletalContributions) {
			Entity* entity = ResolveEntity(contribution.entityGuid);
			if (!entity) {
				continue;
			}
			auto* animation = entity->GetComponent<
				SkeletalAnimationComponent>();
			if (!animation) {
				continue;
			}

			if (contribution.playState && !contribution.stateId.empty()) {
				(void)animation->PlayState(contribution.stateId);
			}
			if (contribution.hasSpeed) {
				if (contribution.layerIndex == 0) {
					animation->SetSpeed(contribution.speed);
				} else if (const auto* desc = animation->GetLayerDesc(
					static_cast<size_t>(contribution.layerIndex)
				)) {
					SkeletalAnimationComponent::AnimationLayerDesc next = *desc;
					next.speed = contribution.speed;
					(void)animation->SetLayerDesc(
						static_cast<size_t>(contribution.layerIndex),
						next
					);
				}
			}
			if (contribution.hasWeight) {
				(void)animation->SetLayerWeight(
					static_cast<size_t>(contribution.layerIndex),
					contribution.weight
				);
			}
			if (contribution.hasPlayback) {
				(void)animation->SetLayerPlaybackTime(
					static_cast<size_t>(contribution.layerIndex),
					contribution.playbackTime
				);
			}
			if (contribution.stopLayer) {
				(void)animation->SetLayerPlaying(
					static_cast<size_t>(contribution.layerIndex),
					false
				);
			}
		}
	}

	void SequenceRuntime::DispatchEvents() {
		if (!mWorld) {
			return;
		}

		GameplayCueBus& cueBus  = mWorld->GetGameplayCueBus();
		ConsoleSystem*  console = mWorld->GetConsoleSystem();
		for (const auto& [keyId, cueId, sourceEntityGuid, cueValue, cueValue2,
			     consoleCommand] : mEventDispatches) {
			(void)keyId;
			if (!cueId.empty() && sourceEntityGuid
			    != 0) {
				GameplayCue cue      = {};
				cue.id               = cueId;
				cue.sourceEntityGuid = sourceEntityGuid;
				cue.value            = cueValue;
				cue.value2           = cueValue2;
				cueBus.Publish(cue);
			}
			if (console && !consoleCommand.empty()) {
				console->ExecuteCommand(
					consoleCommand, EXEC_FLAG::FROM_ENGINE
				);
			}
		}
	}

	void SequenceRuntime::EvaluateEventRange(
		const SequenceTraversalRange&   traversal,
		const SequencePlayer&           player,
		const SequenceBindingAssetData* binding,
		const SequenceSectionAssetData& section,
		const int64_t                   lengthFrames
	) {
		if (!traversal.valid || section.eventKeys.empty()) {
			return;
		}
		if (
			traversal.causedBySeek &&
			player.GetSeekEventPolicy() == SEQUENCE_SEEK_EVENT_POLICY::SUPPRESS
		) {
			return;
		}

		const float length = std::max(0.0f, static_cast<float>(lengthFrames));
		const auto  QueueDispatch = [&](
			const SequenceEventKeyAssetData& key,
			const uint64_t                   rangeSerial
		) {
			const uint64_t guardKey = BuildEventDispatchGuardKey(
				player.GetPlayerId(),
				key.keyId,
				rangeSerial
			);
			if (!mEventDispatchGuard.emplace(guardKey).second) {
				return;
			}

			EventDispatch dispatch    = {};
			dispatch.keyId            = key.keyId;
			dispatch.cueId            = key.cueId;
			dispatch.sourceEntityGuid = key.sourceEntityGuid != 0 ?
				                            key.sourceEntityGuid :
				                            binding ?
				                            binding->entityGuid :
				                            0;
			dispatch.cueValue       = key.cueValue;
			dispatch.cueValue2      = key.cueValue2;
			dispatch.consoleCommand = key.consoleCommand;
			mEventDispatches.emplace_back(std::move(dispatch));
		};

		const auto EmitForwardRange = [&](
			const float    startFrame,
			const float    endFrame,
			const uint64_t rangeSerial
		) {
			for (const SequenceEventKeyAssetData& key : section.eventKeys) {
				const float frame = static_cast<float>(key.frame);
				if (frame > startFrame && frame <= endFrame) {
					QueueDispatch(key, rangeSerial);
				}
			}
		};

		const auto EmitBackwardRange = [&](
			const float    startFrame,
			const float    endFrame,
			const uint64_t rangeSerial
		) {
			for (const SequenceEventKeyAssetData& key : section.eventKeys) {
				const float frame = static_cast<float>(key.frame);
				if (frame >= endFrame && frame < startFrame) {
					QueueDispatch(key, rangeSerial);
				}
			}
		};

		if (!traversal.wrapped || traversal.loopCount <= 0 || length <= 0.0f) {
			if (traversal.direction == SEQUENCE_PLAYBACK_DIRECTION::FORWARD) {
				EmitForwardRange(
					traversal.startFrame,
					traversal.endFrame,
					traversal.traversalSerial
				);
			} else {
				EmitBackwardRange(
					traversal.startFrame,
					traversal.endFrame,
					traversal.traversalSerial
				);
			}
			return;
		}

		uint64_t segmentSerial = traversal.traversalSerial;
		if (traversal.direction == SEQUENCE_PLAYBACK_DIRECTION::FORWARD) {
			EmitForwardRange(traversal.startFrame, length, segmentSerial++);

			const int32_t fullLoopCount = std::max(0, traversal.loopCount - 1);
			for (int32_t i = 0; i < fullLoopCount; ++i) {
				for (const SequenceEventKeyAssetData& key : section.eventKeys) {
					QueueDispatch(key, segmentSerial++);
				}
			}

			EmitForwardRange(0.0f, traversal.endFrame, segmentSerial++);
			return;
		}

		EmitBackwardRange(traversal.startFrame, 0.0f, segmentSerial++);
		const int32_t fullLoopCount = std::max(0, traversal.loopCount - 1);
		for (int32_t i = 0; i < fullLoopCount; ++i) {
			for (const SequenceEventKeyAssetData& key : section.eventKeys) {
				QueueDispatch(key, segmentSerial++);
			}
		}
		EmitBackwardRange(length, traversal.endFrame, segmentSerial++);
	}

	std::string SequenceRuntime::BuildPropertyKey(
		const uint64_t         entityGuid,
		const std::string_view componentStableName,
		const std::string_view propertyPath
	) {
		std::string key = std::to_string(entityGuid);
		key             += "|";
		key             += std::string(componentStableName);
		key             += "|";
		key             += std::string(propertyPath);
		return key;
	}
}
