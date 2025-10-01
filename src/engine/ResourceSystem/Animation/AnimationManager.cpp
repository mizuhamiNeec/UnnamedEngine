#include "engine/ResourceSystem/Animation/AnimationManager.h"

#include <cassert>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>

#include "engine/OldConsole/Console.h"

void AnimationManager::Init() {
	// 特に何もしない
}

void AnimationManager::Shutdown() {
	// アニメーションのキャッシュをクリア
	animations_.clear();
	fileAnimationNames_.clear();
}

Animation AnimationManager::GetAnimation(const std::string& name) const {
	auto it = animations_.find(name);
	if (it != animations_.end()) {
		return it->second;
	}
	Console::Print(
		std::format("アニメーションが見つかりませんでした: {}", name)
	);
	return {}; // 見つからなかった場合は空のアニメーションを返す
}

Animation AnimationManager::LoadAnimationFile(const std::string& filePath) {
	// キャッシュしているものがあったらそれを返す
	auto it = animations_.find(filePath);
	if (it != animations_.end()) {
		return it->second;
	}

	// キャッシュに無い場合は新たに読み込む
	Animation        animation;
	Assimp::Importer importer;
	const aiScene*   aScene = importer.ReadFile(filePath.c_str(), 0);
	assert(aScene->mNumAnimations != 0 && "No animations found in the file");
	aiAnimation* animationAssimp = aScene->mAnimations[0];
	// 最初のアニメーションだけ採用（既存の互換性のため）
	animation = LoadSingleAnimation(animationAssimp);

	// キャッシュに詰め込む
	animations_[filePath] = animation;

	return animation;
}

std::vector<Animation> AnimationManager::LoadAllAnimationsFromFile(const std::string& filePath) {
	std::vector<Animation> animations;
	
	Assimp::Importer importer;
	const aiScene* aScene = importer.ReadFile(filePath.c_str(), 0);
	assert(aScene && "Failed to load animation file");
	assert(aScene->mNumAnimations != 0 && "No animations found in the file");
	
	// ファイル内のアニメーション名をキャッシュ
	std::vector<std::string> animationNames;
	
	for (uint32_t animIndex = 0; animIndex < aScene->mNumAnimations; ++animIndex) {
		aiAnimation* animationAssimp = aScene->mAnimations[animIndex];
		Animation animation = LoadSingleAnimation(animationAssimp);
		
		// アニメーションに名前をつける
		std::string animationKey = filePath + "::" + std::string(animationAssimp->mName.C_Str());
		animationNames.emplace_back(animationAssimp->mName.C_Str());
		
		// キャッシュに保存
		animations_[animationKey] = animation;
		animations.emplace_back(animation);
	}
	
	// ファイルのアニメーション名リストをキャッシュ
	fileAnimationNames_[filePath] = animationNames;
	
	return animations;
}

Animation AnimationManager::LoadAnimationByName(const std::string& filePath, const std::string& animationName) {
	// キャッシュを確認
	const std::string animationKey = filePath + "::" + animationName;
	auto it = animations_.find(animationKey);
	if (it != animations_.end()) {
		return it->second;
	}
	
	Assimp::Importer importer;
	const aiScene* aScene = importer.ReadFile(filePath.c_str(), 0);
	assert(aScene && "Failed to load animation file");
	assert(aScene->mNumAnimations != 0 && "No animations found in the file");
	
	// 指定された名前のアニメーションを探す
	for (uint32_t animIndex = 0; animIndex < aScene->mNumAnimations; ++animIndex) {
		aiAnimation* animationAssimp = aScene->mAnimations[animIndex];
		if (std::string(animationAssimp->mName.C_Str()) == animationName) {
			Animation animation = LoadSingleAnimation(animationAssimp);
			
			// キャッシュに保存
			animations_[animationKey] = animation;
			return animation;
		}
	}
	
	Console::Print(
		std::format("指定されたアニメーションが見つかりませんでした: {} in {}", animationName, filePath)
	);
	return {};
}

std::vector<std::string> AnimationManager::GetAnimationNamesFromFile(const std::string& filePath) {
	// キャッシュを確認
	auto it = fileAnimationNames_.find(filePath);
	if (it != fileAnimationNames_.end()) {
		return it->second;
	}
	
	std::vector<std::string> animationNames;
	
	Assimp::Importer importer;
	const aiScene* aScene = importer.ReadFile(filePath.c_str(), 0);
	if (!aScene || aScene->mNumAnimations == 0) {
		return animationNames;
	}
	
	for (uint32_t animIndex = 0; animIndex < aScene->mNumAnimations; ++animIndex) {
		aiAnimation* animationAssimp = aScene->mAnimations[animIndex];
		animationNames.emplace_back(std::string(animationAssimp->mName.C_Str()));
	}
	
	// キャッシュに保存
	fileAnimationNames_[filePath] = animationNames;
	
	return animationNames;
}

Animation AnimationManager::LoadSingleAnimation(const aiAnimation* animationAssimp) {
	Animation animation;
	animation.duration = static_cast<float>(
		animationAssimp->mDuration / animationAssimp->mTicksPerSecond
	);

	for (
		uint32_t channelIndex = 0;
		channelIndex < animationAssimp->mNumChannels
		; ++channelIndex
	) {
		aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[
			channelIndex];
		NodeAnimation& nodeAnimation = animation.nodeAnimations[
			nodeAnimationAssimp->mNodeName.C_Str()
		];
		// ノード名をキャッシュに追加
		animation.nodeNames.emplace_back(
			nodeAnimationAssimp->mNodeName.C_Str()
		);
		for (
			uint32_t keyIndex = 0;
			keyIndex < nodeAnimationAssimp->mNumPositionKeys;
			++keyIndex
		) {
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[
				keyIndex];
			Keyframe<Vec3> keyframe;
			keyframe.time = static_cast<float>(keyAssimp.mTime / animationAssimp
				->mTicksPerSecond); // 秒に変換
			keyframe.value = {
				-keyAssimp.mValue.x,
				keyAssimp.mValue.y,
				keyAssimp.mValue.z
			}; // 右手から左手
			nodeAnimation.translate.keyFrames.emplace_back(keyframe);
		}
		for (
			uint32_t keyIndex = 0;
			keyIndex < nodeAnimationAssimp->mNumRotationKeys;
			++keyIndex
		) {
			aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[
				keyIndex];
			Keyframe<Quaternion> keyframe;
			keyframe.time = static_cast<float>(keyAssimp.mTime /
				animationAssimp->mTicksPerSecond);
			keyframe.value = {
				keyAssimp.mValue.x,
				-keyAssimp.mValue.y,
				-keyAssimp.mValue.z,
				keyAssimp.mValue.w
			}; // 右手から左手
			nodeAnimation.rotate.keyFrames.emplace_back(keyframe);
		}
		for (
			uint32_t keyIndex = 0;
			keyIndex < nodeAnimationAssimp->mNumScalingKeys;
			++keyIndex
		) {
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[
				keyIndex];
			Keyframe<Vec3> keyframe;
			keyframe.time = static_cast<float>(keyAssimp.mTime /
				animationAssimp->mTicksPerSecond);
			keyframe.value = {
				keyAssimp.mValue.x,
				keyAssimp.mValue.y,
				keyAssimp.mValue.z
			};
			nodeAnimation.scale.keyFrames.emplace_back(keyframe);
		}
	}

	return animation;
}
