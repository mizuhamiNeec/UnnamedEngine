#pragma once
#include <format>
#include <unordered_map>
#include <vector>

#include <Animation/Animation.h>

#include <SubSystem/Console/Console.h>

// Forward declaration
struct aiAnimation;

class AnimationManager {
public:
	void Init();

	void Shutdown();

	[[nodiscard]] Animation GetAnimation(const std::string& name) const {
		auto it = animations_.find(name);
		if (it != animations_.end()) {
			return it->second;
		}
		Console::Print(
			std::format("アニメーションが見つかりませんでした: {}", name)
		);
		return {}; // 見つからなかった場合は空のアニメーションを返す
	}

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
	Animation LoadSingleAnimation(aiAnimation* animationAssimp);
};
