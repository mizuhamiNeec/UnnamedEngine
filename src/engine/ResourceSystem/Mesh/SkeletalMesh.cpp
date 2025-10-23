#include <engine/ResourceSystem/Mesh/SkeletalMesh.h>

/// @brief コンストラクタ
/// @param name メッシュ名
SkeletalMesh::SkeletalMesh(std::string name) : name_(std::move(name)) {
}

/// @brief デストラクタ
SkeletalMesh::~SkeletalMesh() = default;

/// @brief サブメッシュの追加
/// @param subMesh 追加するサブメッシュ
void SkeletalMesh::AddSubMesh(std::unique_ptr<SubMesh> subMesh) {
	subMeshes_.emplace_back(std::move(subMesh));
}

/// @brief サブメッシュの取得
/// @return サブメッシュのリスト
const std::vector<std::unique_ptr<SubMesh>>& SkeletalMesh::
GetSubMeshes() const {
	return subMeshes_;
}

/// @brief メッシュ名の取得
/// @return メッシュ名
std::string SkeletalMesh::GetName() const {
	return name_;
}

/// @brief スケルトンの設定
/// @param skeleton スケルトン
void SkeletalMesh::SetSkeleton(const Skeleton& skeleton) {
	skeleton_ = skeleton;
}

/// @brief スケルトンの取得
/// @return スケルトン
const Skeleton& SkeletalMesh::GetSkeleton() const {
	return skeleton_;
}

/// @brief アニメーションの追加
/// @param name アニメーション名
/// @param animation アニメーション
void SkeletalMesh::AddAnimation(const std::string& name,
                                const Animation&   animation) {
	animations_[name] = animation;
}

/// @brief アニメーションの取得
/// @param name アニメーション名
/// @return アニメーションへのポインタ（存在しない場合はnullptr）
const Animation* SkeletalMesh::GetAnimation(const std::string& name) const {
	auto it = animations_.find(name);
	return it != animations_.end() ? &it->second : nullptr;
}

/// @brief すべてのアニメーションの取得
/// @return アニメーションのマップ	
const std::map<std::string, Animation>& SkeletalMesh::GetAnimations() const {
	return animations_;
}

/// @brief メッシュのレンダリング
/// @param commandList コマンドリスト
void SkeletalMesh::Render(ID3D12GraphicsCommandList* commandList) const {
	for (const auto& subMesh : subMeshes_) {
		subMesh->Render(commandList);
	}
}

/// @brief メッシュリソースの解放
void SkeletalMesh::ReleaseResource() {
	for (const auto& subMesh : subMeshes_) {
		subMesh->ReleaseResource();
	}
	subMeshes_.clear();
	animations_.clear();
}
