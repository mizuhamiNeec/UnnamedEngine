#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/assets/types/SequenceAssetData.h"
#include "core/math/Quaternion.h"
#include "core/math/Vec3.h"

#include "PreAnimatedStateStore.h"
#include "SequencePropertyAccessorRegistry.h"
#include "SequenceRuntimeTypes.h"

namespace Unnamed {
	class Entity;
	class SequencePlayer;
	class World;

	/// @brief シーケンス再生をワールド単位で管理するランタイムです。
	class SequenceRuntime final {
	public:
		/// @brief コンストラクタです。
		explicit SequenceRuntime(World* world = nullptr);

		/// @brief ワールド参照を設定します。
		void SetWorld(World* world);

		/// @brief 再生プレイヤーを登録します。
		void RegisterPlayer(const std::shared_ptr<SequencePlayer>& player);

		/// @brief 再生プレイヤーを登録解除します。
		void UnregisterPlayer(uint64_t playerId);

		/// @brief 固定ティック前フェーズで評価・適用します。
		void AdvanceAndApplyPreSimulation(float deltaSeconds);

		/// @brief 固定ティック後フェーズで評価・適用します。
		void ApplyPostSimulation();

		/// @brief エディタ用の評価更新を行います。
		void EditorTick(float deltaSeconds);

		/// @brief ランタイム状態をすべてクリアします。
		void Clear();

	private:
		/// @brief Float寄与情報です。
		struct FloatContribution final {
			uint64_t            entityGuid = 0;
			std::string         componentStableName;
			std::string         propertyPath;
			float               value     = 0.0f;
			int32_t             priority  = 0;
			SEQUENCE_BLEND_MODE blendMode = SEQUENCE_BLEND_MODE::MODE_ABSOLUTE;
			uint64_t            order     = 0;
		};

		/// @brief Bool寄与情報です。
		struct BoolContribution final {
			uint64_t    entityGuid = 0;
			std::string componentStableName;
			std::string propertyPath;
			bool        value    = false;
			int32_t     priority = 0;
			uint64_t    order    = 0;
		};

		/// @brief Vec3寄与情報です。
		struct Vec3Contribution final {
			uint64_t            entityGuid = 0;
			std::string         componentStableName;
			std::string         propertyPath;
			Vec3                value     = Vec3::zero;
			int32_t             priority  = 0;
			SEQUENCE_BLEND_MODE blendMode = SEQUENCE_BLEND_MODE::MODE_ABSOLUTE;
			uint64_t            order     = 0;
		};

		/// @brief Transform寄与情報です。
		struct TransformContribution final {
			uint64_t            entityGuid = 0;
			bool                hasPosition = false;
			Vec3                position = Vec3::zero;
			bool                hasRotation = false;
			Quaternion          rotation = Quaternion::identity;
			bool                hasScale = false;
			Vec3                scale = Vec3::one;
			int32_t             priority = 0;
			SEQUENCE_BLEND_MODE blendMode = SEQUENCE_BLEND_MODE::MODE_ABSOLUTE;
			uint64_t            order = 0;
		};

		/// @brief Entity bool寄与情報です。
		struct EntityBoolContribution final {
			uint64_t entityGuid = 0;
			bool     value      = false;
			int32_t  priority   = 0;
			uint64_t order      = 0;
		};

		/// @brief カメラカット寄与情報です。
		struct CameraContribution final {
			uint64_t cameraEntityGuid = 0;
			int32_t  priority         = 0;
			uint64_t order            = 0;
		};

		/// @brief スケルタル制御寄与情報です。
		struct SkeletalContribution final {
			uint64_t    entityGuid   = 0;
			int32_t     layerIndex   = 0;
			bool        hasWeight    = false;
			float       weight       = 0.0f;
			bool        hasPlayback  = false;
			float       playbackTime = 0.0f;
			bool        hasSpeed     = false;
			float       speed        = 1.0f;
			bool        playState    = false;
			std::string stateId;
			bool        stopLayer = false;
			int32_t     priority  = 0;
			uint64_t    order     = 0;
		};

		/// @brief EventDispatchは、sequence eventの発火時刻、event名、payloadを保持します
		struct EventDispatch final {
			uint64_t    keyId = 0;
			std::string cueId;
			uint64_t    sourceEntityGuid = 0;
			float       cueValue         = 0.0f;
			float       cueValue2        = 0.0f;
			std::string consoleCommand;
		};

		/// @brief FloatTargetMetaは、sequenceのFloatTarget対象を実行時に解決する識別情報を保持します
		struct FloatTargetMeta final {
			uint64_t    entityGuid = 0;
			std::string componentStableName;
			std::string propertyPath;
		};

		/// @brief BoolTargetMetaは、sequenceのBoolTarget対象を実行時に解決する識別情報を保持します
		struct BoolTargetMeta final {
			uint64_t    entityGuid = 0;
			std::string componentStableName;
			std::string propertyPath;
		};

		/// @brief Vec3TargetMetaは、sequenceのVec3Target対象を実行時に解決する識別情報を保持します
		struct Vec3TargetMeta final {
			uint64_t    entityGuid = 0;
			std::string componentStableName;
			std::string propertyPath;
		};

		[[nodiscard]] Entity* ResolveEntity(uint64_t entityGuid) const;
		[[nodiscard]] const SequenceBindingAssetData* FindBinding(
			const SequenceAssetData& asset,
			uint64_t                 bindingId
		) const;

		void AdvancePlayers(float deltaSeconds);
		void EvaluatePreSimulation();
		void EvaluatePostSimulation();
		void RestoreUntouchedState();
		void PruneExpiredPlayers();

		void CollectTrackPre(
			const SequencePlayer&           player,
			const SequenceAssetData&        asset,
			const SequenceTrackAssetData&   track,
			const SequenceBindingAssetData* binding,
			uint64_t                        playerOrder
		);

		void CollectTrackPost(
			SequencePlayer&                 player,
			const SequenceAssetData&        asset,
			const SequenceTrackAssetData&   track,
			const SequenceBindingAssetData* binding,
			uint64_t                        playerOrder
		);

		void ApplyFloatContributions();
		void ApplyBoolContributions();
		void ApplyVec3Contributions();
		void ApplyTransformContributions();
		void ApplyEntityVisibilityContributions();
		void ApplyEntityActivationContributions();
		void ApplyCameraContributions();
		void ApplySkeletalContributions();
		void DispatchEvents();

		/// @brief イベント区間を走査して発火キューへ積みます。
		void EvaluateEventRange(
			const SequenceTraversalRange&   traversal,
			const SequencePlayer&           player,
			const SequenceBindingAssetData* binding,
			const SequenceSectionAssetData& section,
			int64_t                         lengthFrames
		);

		[[nodiscard]] static std::string BuildPropertyKey(
			uint64_t         entityGuid,
			std::string_view componentStableName,
			std::string_view propertyPath
		);

		World*                           mWorld            = nullptr;
		SequencePropertyAccessorRegistry mAccessorRegistry = {};
		PreAnimatedStateStore            mPreAnimatedState = {};

		std::unordered_map<uint64_t, std::weak_ptr<SequencePlayer>> mPlayers =
			{};

		std::unordered_map<std::string, std::vector<FloatContribution>>
		mFloatContributions = {};
		std::unordered_map<std::string, std::vector<BoolContribution>>
		mBoolContributions = {};
		std::unordered_map<std::string, std::vector<Vec3Contribution>>
		mVec3Contributions = {};
		std::unordered_map<uint64_t, std::vector<TransformContribution>>
		mTransformContributions = {};
		std::unordered_map<uint64_t, std::vector<EntityBoolContribution>>
		mVisibilityContributions = {};
		std::unordered_map<uint64_t, std::vector<EntityBoolContribution>>
		mActivationContributions                                 = {};
		std::vector<CameraContribution>   mCameraContributions   = {};
		std::vector<SkeletalContribution> mSkeletalContributions = {};
		std::vector<EventDispatch>        mEventDispatches       = {};
		std::unordered_set<uint64_t>      mEventDispatchGuard    = {};

		std::unordered_map<std::string, FloatTargetMeta> mFloatTargetMeta = {};
		std::unordered_map<std::string, BoolTargetMeta>  mBoolTargetMeta  = {};
		std::unordered_map<std::string, Vec3TargetMeta>  mVec3TargetMeta  = {};

		std::unordered_set<std::string> mTouchedFloatKeys  = {};
		std::unordered_set<std::string> mTouchedBoolKeys   = {};
		std::unordered_set<std::string> mTouchedVec3Keys   = {};
		std::unordered_set<uint64_t>    mTouchedTransforms = {};
		std::unordered_set<uint64_t>    mTouchedVisibility = {};
		std::unordered_set<uint64_t>    mTouchedActivation = {};
		bool                            mTouchedCamera     = false;

		uint64_t mContributionOrderCounter = 0;
	};
}
