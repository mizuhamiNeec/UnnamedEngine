
#pragma once
#include <vector>
#include <math/public/MathLib.h>
#include "math/public/Quaternion.h"
#include <engine/public/Animation/KeyFrame.h>

struct aiNode;

template <typename T>
struct AnimationCurve {
	std::vector<Keyframe<T>> keyFrames;
};

struct NodeAnimation {
	AnimationCurve<Vec3>       translate;
	AnimationCurve<Quaternion> rotate;
	AnimationCurve<Vec3>       scale;
};

struct Node {
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
