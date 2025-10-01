#pragma once
#include <format>
#include <unordered_map>
#include <vector>

#include <engine/Animation/Animation.h>

struct aiAnimation;

class AnimationManager {
public:
	void Init();

	void Shutdown();

	[[nodiscard]] Animation GetAnimation(const std::string& name) const;

	// 単一アニメーション読み込み
	Animation LoadAnimationFile(const std::string& filePath);

	// 複数アニメーション読み込み
	std::vector<Animation> LoadAllAnimationsFromFile(
		const std::string& filePath);

	// ファイル内の特定のアニメーションを名前で読み込み
	Animation LoadAnimationByName(const std::string& filePath,
	                              const std::string& animationName);

	// ファイル内のアニメーション名一覧を取得
	std::vector<std::string> GetAnimationNamesFromFile(
		const std::string& filePath);

private:
	std::unordered_map<std::string, Animation> animations_;
	// ファイルパス/アニメーション名
	std::unordered_map<std::string, std::vector<std::string>>
	fileAnimationNames_;
	// アニメーション名/アニメーション

	// アニメーション読み込みのヘルパー関数
	static Animation LoadSingleAnimation(const aiAnimation* animationAssimp);
};
