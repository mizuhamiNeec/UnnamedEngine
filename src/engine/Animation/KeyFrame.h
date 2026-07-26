#pragma once
#include <vector>

/// @brief Keyframeは、animation sampleの時刻と補間対象値を1 keyとして保持します
template <typename T>
struct Keyframe {
	float time;
	T     value;
};

using KeyframeVec3       = Keyframe<Vec3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

Vec3 CalculateValue(
	const std::vector<KeyframeVec3>& keyframes,
	float                            time
);

Quaternion CalculateValue(
	const std::vector<KeyframeQuaternion>& keyframes, float time
);
