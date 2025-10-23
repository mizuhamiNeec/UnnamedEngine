#pragma once
#include <memory>
#include <string>
#include <vector>
#include <map>

#include <engine/ResourceSystem/Mesh/SubMesh.h>

#include "engine/Animation/Animation.h"

// ボーン情報を格納する構造体
struct Bone {
	std::string name;
	int         id;
	Mat4        offsetMatrix; // ボーンのオフセット行列
};

// スケルトン情報を格納する構造体
struct Skeleton {
	std::vector<Bone>          bones;
	std::map<std::string, int> boneMap;      // ボーン名からIDへのマップ
	std::vector<Mat4>          boneMatrices; // ボーン変換行列
	Node                       rootNode;
};

/// @brief スケルタルメッシュクラス
class SkeletalMesh {
public:
	explicit SkeletalMesh(std::string name);

	~SkeletalMesh();

	void AddSubMesh(std::unique_ptr<SubMesh> subMesh);
	[[nodiscard]] const std::vector<std::unique_ptr<SubMesh>>&
	GetSubMeshes() const;

	[[nodiscard]] std::string GetName() const;

	// スケルトン関連
	void                          SetSkeleton(const Skeleton& skeleton);
	[[nodiscard]] const Skeleton& GetSkeleton() const;

	// アニメーション関連
	void AddAnimation(const std::string& name, const Animation& animation);
	[[nodiscard]] const Animation* GetAnimation(const std::string& name) const;
	[[nodiscard]] const std::map<std::string, Animation>&
	GetAnimations() const;

	void Render(ID3D12GraphicsCommandList* commandList) const;

	void ReleaseResource();

private:
	std::string                           name_;
	std::vector<std::unique_ptr<SubMesh>> subMeshes_;
	Skeleton                              skeleton_;
	std::map<std::string, Animation>      animations_;
};
