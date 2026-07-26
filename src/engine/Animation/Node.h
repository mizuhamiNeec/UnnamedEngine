#pragma once
#include <vector>

#include <engine/Animation/KeyFrame.h>

#include "core/math/Mat4.h"
#include "core/math/Quaternion.h"

struct aiNode;

/// @brief AnimationCurveは、時間順keyframe列を所有し、指定時刻の値を補間します
template <typename T>
struct AnimationCurve {
	std::vector<Keyframe<T>> keyFrames;
};

/// @brief NodeAnimationは、skeleton nodeごとのtranslation、rotation、scale curveを保持します
struct NodeAnimation {
	AnimationCurve<Vec3>       translate;
	AnimationCurve<Quaternion> rotate;
	AnimationCurve<Vec3>       scale;
};

/// @brief アニメーション階層の子ノードとローカル変換を保持します
struct Node {
	/// @brief アニメーションキーから得た平行移動、回転、拡縮を一組で保持します
	struct Transform {
		Vec3       translate;
		Quaternion rotate;
		Vec3       scale;
	} transform;

	Mat4              localMat;
	std::string       name;
	std::vector<Node> children;
};

Node ReadNode(const aiNode* node);
