#pragma once
#include <map>
#include <string>
#include <engine/Animation/Node.h>

struct Animation {
	float duration; // アニメーション全体の尺
	// NodeAnimationの集合。Node名でひけるようにしておく
	std::map<std::string, NodeAnimation> nodeAnimations;
	std::vector<std::string>             nodeNames; // ノード名のリスト
};
