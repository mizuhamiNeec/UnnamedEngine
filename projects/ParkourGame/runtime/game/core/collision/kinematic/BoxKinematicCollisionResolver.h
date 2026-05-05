#pragma once
#include <array>

#include "base/BaseKinematicCollisionResolver.h"

namespace Unnamed {
	class BoxKinematicCollisionResolver final : public
		BaseKinematicCollisionResolver {
	public:
		/// @brief 重なり復帰処理の呼び出し理由を表します。
		enum class RECOVER_REASON : uint8_t {
			NONE = 0,
			SLIDE_START,
			HIT_START_SOLID,
			HIT_ALL_SOLID,
			END_POSITION_OVERLAP,
		};

		struct CollisionDebugState {
			bool          hasBlockingHit    = false;
			uint32_t      blockingHitCount  = 0;
			Vec3          blockingCastStart = Vec3::zero;
			Vec3          blockingCastDir   = Vec3::zero;
			float         blockingCastLength = 0.0f;
			Physics::Hit  blockingHit       = {};

			bool          recoverAttempted  = false;
			bool          recoverSucceeded  = false;
			RECOVER_REASON recoverReason    = RECOVER_REASON::NONE;
			Vec3          recoverStart      = Vec3::zero;
			Vec3          recoverEnd        = Vec3::zero;
			int           recoverOverlapCount = 0;
			std::array<Physics::Hit, 16> recoverOverlaps = {};

			Vec3 startPosition = Vec3::zero;
			Vec3 endPosition   = Vec3::zero;
			Vec3 inputVelocity = Vec3::zero;
			Vec3 outputVelocity = Vec3::zero;
		};

		using BaseKinematicCollisionResolver::BaseKinematicCollisionResolver;

		/// @brief 衝突判定用のボックスを更新します。
		void UpdateHull(
			const Vec3& pos, const Vec3& halfExtents
		);

		/// @brief スライド移動を実行します。
		void SlideMove(
			Vec3& position, Vec3& velocity, float timeTotal
		) const override;

		/// @brief 段差移動を実行します。
		void StepMove(
			Vec3& position, Vec3& velocity, float stepHeight, float timeTotal
		) const override;

		/// @brief 接地判定を行います。
		bool ProbeGround(
			const Vec3& position, float maxDistance, Physics::Hit* outHit
		) const override;

		/// @brief 指定位置での重なり候補を収集します。
		int CollectOverlaps(
			const Vec3& position,
			const Vec3& halfExtents,
			Physics::Hit* outHits,
			int           maxHits
		) const override;

		/// @brief 直近の衝突応答デバッグ情報を取得します。
		[[nodiscard]] const CollisionDebugState& GetCollisionDebugState() const;

	private:
		/// @brief スイープ衝突情報をデバッグ状態へ記録します。
		void RecordBlockingHit(
			const Vec3&       castStart,
			const Vec3&       castDir,
			float             castLength,
			const Physics::Hit& hit
		) const;

		/// @brief 指定位置での重なりを検査します。
		[[nodiscard]] bool IsHullOverlapping(
			const Vec3&    position,
			Physics::Hit*  outHit = nullptr
		) const;

		/// @brief 開始重なり時に押し出しと近傍探索で復帰を試みます。
		[[nodiscard]] bool RecoverFromPenetration(
			Vec3&       position,
			const Vec3& preferredDirection,
			RECOVER_REASON reason
		) const;

		static float CastSkinM();
		/// @brief 接触判定で使うスキン幅を返します。
		static float SkinM();

		Box mHull;
		mutable CollisionDebugState mCollisionDebugState = {};
	};
}
