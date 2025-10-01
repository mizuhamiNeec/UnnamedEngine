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
	std::vector<Bone>                bones;
	std::map<std::string, int>       boneMap; // ボーン名からIDへのマップ
	std::vector<Mat4>                boneMatrices; // ボーン変換行列
	Node                             rootNode;
};

class SkeletalMesh {
public:
	explicit SkeletalMesh(std::string name) : name_(std::move(name)) {
	}
	~SkeletalMesh() = default;

	void AddSubMesh(std::unique_ptr<SubMesh> subMesh) {
		subMeshes_.emplace_back(std::move(subMesh));
	}

	[[nodiscard]] const std::vector<std::unique_ptr<SubMesh>>&
	GetSubMeshes() const {
		return subMeshes_;
	}

	[[nodiscard]] std::string GetName() const {
		return name_;
	}

	// スケルトン関連
	void SetSkeleton(const Skeleton& skeleton) {
		skeleton_ = skeleton;
	}

	[[nodiscard]] const Skeleton& GetSkeleton() const {
		return skeleton_;
	}

	// アニメーション関連
	void AddAnimation(const std::string& name, const Animation& animation) {
		animations_[name] = animation;
	}

	[[nodiscard]] const Animation* GetAnimation(const std::string& name) const {
		auto it = animations_.find(name);
		return it != animations_.end() ? &it->second : nullptr;
	}

	[[nodiscard]] const std::map<std::string, Animation>& GetAnimations() const {
		return animations_;
	}

	void Render(ID3D12GraphicsCommandList* commandList) const {
		for (const auto& subMesh : subMeshes_) {
			subMesh->Render(commandList);
		}
	}

	void ReleaseResource() {
		for (const auto& subMesh : subMeshes_) {
			subMesh->ReleaseResource();
		}
		subMeshes_.clear();
		animations_.clear();
	}

private:
	std::string                           name_;
	std::vector<std::unique_ptr<SubMesh>> subMeshes_;
	Skeleton                              skeleton_;
	std::map<std::string, Animation>      animations_;
};
