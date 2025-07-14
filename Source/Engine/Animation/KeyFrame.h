#pragma once
#include <cassert>
#include <vector>

#include <Lib/Math/Quaternion/Quaternion.h>
#include <Lib/Math/Vector/Vec3.h>

#include "Lib/Math/MathLib.h"

#include "SubSystem/Console/Console.h"

template <typename T>
struct Keyframe {
	float time;
	T     value;
};

using KeyframeVec3       = Keyframe<Vec3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

namespace {
	Vec3 CalculateValue(const std::vector<KeyframeVec3>& keyframes,
	                    float                            time) {
		//assert(!keyframes.empty() && "Keyframes must not be empty");
		if (keyframes.empty()) {
			Console::Print(
				"キーがありません。\n"
			);
			return {};
		}
		if (keyframes.size() == 1 || time <= keyframes[0].time) {
			// キーが1つか、時刻がキーフレーム前なら最初の値とする
			return keyframes[0].value; // キーが1つしかない場合はその値を返す
		}

		for (size_t index = 0; index < keyframes.size() - 1; ++index) {
			size_t nextIndex = index + 1;
			// indexとnextIndexの２つのkeyframeを取得して範囲内に時刻があるかを判定
			if (
				keyframes[index].time <= time &&
				time <= keyframes[nextIndex].time
			) {
				float t = (time - keyframes[index].time) / (keyframes[nextIndex]
					.
					time - keyframes[index].time);
				return Math::Lerp(
					keyframes[index].value,
					keyframes[nextIndex].value,
					t
				);
			}
		}
		// ここまで来た場合は最後の時刻よりも後ろなので最後の値を返すことになる
		return keyframes.rbegin()->value;
	}

	Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes,
	                          float                                  time) {
		//assert(!keyframes.empty() && "Keyframes must not be empty");
		if (keyframes.empty()) {
			Console::Print(
				"キーがありません。\n"
			);
			return {};
		}
		if (keyframes.size() == 1 || time <= keyframes[0].time) {
			// キーが1つか、時刻がキーフレーム前なら最初の値とする
			return keyframes[0].value; // キーが1つしかない場合はその値を返す
		}

		for (size_t index = 0; index < keyframes.size() - 1; ++index) {
			size_t nextIndex = index + 1;
			// indexとnextIndexの２つのkeyframeを取得して範囲内に時刻があるかを判定
			if (
				keyframes[index].time <= time &&
				time <= keyframes[nextIndex].time
			) {
				float t = (time - keyframes[index].time) / (keyframes[nextIndex]
					.
					time - keyframes[index].time);
				return Quaternion::Slerp(
					keyframes[index].value,
					keyframes[nextIndex].value,
					t
				);
			}
		}
		// ここまで来た場合は最後の時刻よりも後ろなので最後の値を返すことになる
		return keyframes.rbegin()->value;
	}
}
