#pragma once

struct Vec4;
struct Vec2;
struct Vec3;
struct Quaternion;

namespace Unnamed {
	template <typename TValue>
	struct TweenLerp;

	/// @brief float値を線形補間してTweenInstanceへ返します
	template <>
	struct TweenLerp<float> {
		static float Evaluate(
			const float& startValue, const float& endValue, float t
		);
	};

	/// @brief double値を線形補間してTweenInstanceへ返します
	template <>
	struct TweenLerp<double> {
		static double Evaluate(
			const double& startValue, const double& endValue, float t
		);
	};

	/// @brief Vec2の各成分を線形補間してTweenInstanceへ返します
	template <>
	struct TweenLerp<Vec2> {
		static Vec2 Evaluate(
			const Vec2& startValue, const Vec2& endValue, float t
		);
	};

	/// @brief Vec3の各成分を線形補間してTweenInstanceへ返します
	template <>
	struct TweenLerp<Vec3> {
		static Vec3 Evaluate(
			const Vec3& startValue, const Vec3& endValue, float t
		);
	};

	/// @brief Vec4の各成分を線形補間してTweenInstanceへ返します
	template <>
	struct TweenLerp<Vec4> {
		static Vec4 Evaluate(
			const Vec4& startValue, const Vec4& endValue, float t
		);
	};

	/// @brief Quaternionを球面線形補間してTweenInstanceへ返します
	template <>
	struct TweenLerp<Quaternion> {
		static Quaternion Evaluate(
			const Quaternion& startValue, const Quaternion& endValue,
			float             t
		);
	};
}
