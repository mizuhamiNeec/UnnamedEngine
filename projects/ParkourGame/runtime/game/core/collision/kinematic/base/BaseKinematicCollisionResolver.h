#pragma once
#include <array>
#include <cstdint>

#include "core/math/Math.h"
#include "core/math/Vec3.h"

#include "engine/physics/core/Physics.h"

namespace Unnamed {
	enum class COLLIDER_TYPE : uint8_t {
		BOX,     // AABB
		SPHERE,  // 球
		CAPSULE, // カプセル
	};

	struct KinematicContact {
		uint64_t hitEntityGuid = 0;
		Vec3     normal        = Vec3::zero;
		Vec3     point         = Vec3::zero;
		float    depth         = 0.0f;
	};

	struct KinematicMoveQuery {
		Vec3 position    = Vec3::zero;
		Vec3 velocity    = Vec3::zero;
		Vec3 halfExtents = Vec3(
			Math::HtoM(32.0f),
			Math::HtoM(73.0f),
			Math::HtoM(32.0f)
		);
		float deltaTime  = 0.0f;
		float skinWidth  = Math::HtoM(0.01f);
		float overBounce = 1.001f;
	};

	struct KinematicMoveResult {
		static constexpr uint32_t kMaxContacts = 4;

		Vec3                                       position     = Vec3::zero;
		Vec3                                       velocity     = Vec3::zero;
		bool                                       collided     = false;
		bool                                       grounded     = false;
		float                                      moveTime     = 0.0f;
		std::array<KinematicContact, kMaxContacts> contacts     = {};
		uint32_t                                   contactCount = 0;
	};

	/// @brief 衝突解決の基底クラスです。
	class BaseKinematicCollisionResolver {
	public:
		explicit BaseKinematicCollisionResolver(Physics::Engine* engine);
		virtual  ~BaseKinematicCollisionResolver();

		/// @brief ベクトルを法線に沿って反射させます。
		/// @param in 入力ベクトル
		/// @param normal 法線ベクトル
		/// @param overbounce 反射のオーバーバウンス係数 (1.0f で完全な反射、1.0f より大きい値でオーバーバウンス)
		/// @return 反射されたベクトル
		static Vec3 ClipVelocity(
			const Vec3& in, const Vec3& normal, float overbounce
		);

		/// @brief 衝突を考慮して移動を行います。
		/// @param position 開始位置（移動後の位置もここに書き込まれます）
		/// @param velocity 開始速度（移動後の速度もここに書き込まれます）
		/// @param timeTotal 移動に使用する時間（秒）
		virtual void SlideMove(
			Vec3& position, Vec3& velocity, float timeTotal
		) const = 0;

		/// @brief ステップ移動を行います。段差やスロープを登り降りするための移動処理です。
		/// @param position 開始位置（移動後の位置もここに書き込まれます）
		/// @param velocity 開始速度（移動後の速度もここに書き込まれます）
		/// @param stepHeightHu 登れる段差の高さ [Hammer Unit]
		/// @param timeTotal 移動に使用する時間（秒）
		virtual void StepMove(
			Vec3& position, Vec3& velocity, float stepHeightHu, float timeTotal
		) const = 0;

		/// @brief 指定位置の直下に支持面があるかを調べます。
		/// @param position 判定開始位置
		/// @param maxDistance 下方向の最大判定距離 [m]
		/// @param outHit ヒット情報の出力先
		/// @return 何かにヒットしたら true
		virtual bool ProbeGround(
			const Vec3& position, float maxDistance, Physics::Hit* outHit
		) const = 0;

		/// @brief 指定ボックスの重なりを収集します。
		/// @param position 判定ボックス中心
		/// @param halfExtents 判定ボックス半径
		/// @param outHits ヒット情報出力先
		/// @param maxHits 出力上限
		/// @return 収集されたヒット数
		virtual int CollectOverlaps(
			const Vec3&   position,
			const Vec3&   halfExtents,
			Physics::Hit* outHits,
			int           maxHits
		) const = 0;

		[[nodiscard]] Physics::Engine* GetPhysics() const;

	protected:
		Physics::Engine* mEngine = nullptr;
	};
}
