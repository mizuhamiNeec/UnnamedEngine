#pragma once
#include <map>
#include <string>
#include <engine/Animation/Node.h>

/// @brief Animationは、animation clipのduration、tick rate、node channel列を所有します
struct Animation {
	float duration; // アニメーション全体の尺
	// NodeAnimationの集合。Node名でひけるようにしておく
	std::map<std::string, NodeAnimation> nodeAnimations;
	std::vector<std::string>             nodeNames; // ノード名のリスト
};
