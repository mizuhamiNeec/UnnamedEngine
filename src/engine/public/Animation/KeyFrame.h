
#pragma once
#include <vector>
#include <runtime/core/math/Math.h>

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

Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes,
						  float                                  time);
