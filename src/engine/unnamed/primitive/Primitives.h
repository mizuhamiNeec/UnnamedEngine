#pragma once

namespace Unnamed {
	/// @brief レイ構造体
	struct Ray {
		Vec3  origin;         // Origin
		Vec3  dir;            // Direction
		Vec3  invDir;         // 1 / dir
		float tMin = 0.0f;    // Minimum distance
		float tMax = FLT_MAX; // Maximum distance
	};

	/// @brief 線分構造体
	struct Line {
		Vec3 start = Vec3::zero;
		Vec3 end   = Vec3::right;
	};

	/// @brief 三角形構造体
	struct Triangle {
		static constexpr float kSin60 = 0.86603f; // 60度の正弦値

		Vec3 v0 = Vec3(-kSin60, -0.5f, 0.0f);
		Vec3 v1 = Vec3(kSin60, -0.5f, 0.0f);
		Vec3 v2 = Vec3::up;
	};

	/// @brief 平面構造体
	struct Plane {
		Vec3  normal = Vec3::zero;
		float d      = 0.0f;
	};

	/// @brief 平面を正規化します
	/// @param plane 正規化する平面
	/// @return 正規化された平面
	Plane NormalizePlane(const Plane& plane);

	/// @brief ボックス構造体
	struct Box {
		Vec3 center   = Vec3::zero;
		Vec3 halfSize = Vec3::one * 0.5f;
	};

	/// @brief フラスタム構造体
	struct Frustum {
		Plane planes[6];
	};

	/// @brief 球 構造体
	struct Sphere {
		Vec3  center = Vec3::zero;
		float radius = 0.5f;
	};

	/// @brief ビュープロジェクション行列からフラスタムを構築します
	/// @param viewProjRowVector ビュープロジェクション行列（行ベクトル形式）
	/// @return 構築されたフラスタム
	Frustum BuildFrustum(const Mat4& viewProjRowVector);

	/// @brief Axis Aligned Bounding Box
	struct AABB {
		Vec3 min = Vec3(FLT_MAX);
		Vec3 max = Vec3(-FLT_MAX);

		void Expand(const Vec3& point);
		void Expand(const AABB& aabb);

		[[nodiscard]] Vec3 Center() const;

		[[nodiscard]] float SurfaceArea() const;
		[[nodiscard]] int   LongestAxis() const;

		[[nodiscard]] Vec3 Size() const;
	};

	AABB   TransformAABB(const AABB& local, const Mat4& world);
	Sphere BuildBoundingSphere(const AABB& aabb);

	bool IsAABBOutsidePlane(const AABB& aabb, const Plane& p);
	bool IsVisible(const Frustum& f, const AABB& worldAABB);
	bool IsVisible(const Frustum& f, const Sphere& worldSphere);
	bool IsVisible(const Mat4& viewProj, const AABB& worldAABB);
	bool IsVisiblePerspective(
		const Sphere& worldSphere,
		const Mat4&   view,
		const Mat4&   proj,
		float         nearZ,
		float         farZ
	);

	/// @brief カプセル構造体
	struct Capsule {
		Vec3  start  = Vec3::down * 0.5f;
		Vec3  end    = Vec3::up * 0.5f;
		float radius = 0.5f;
	};
}
