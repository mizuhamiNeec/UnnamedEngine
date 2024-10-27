#include "ModelManager.h"

#include "Model.h"
#include "ModelCommon.h"

#include "../Renderer/ConstantBuffer.h"
#include "../Renderer/VertexBuffer.h"

ModelManager* ModelManager::instance_ = nullptr;

// HACK : 要修正
// TODO : シングルトンは悪!!
ModelManager* ModelManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new ModelManager;
	}
	return instance_;
}

//-----------------------------------------------------------------------------
// Purpose: 初期化します
//-----------------------------------------------------------------------------
void ModelManager::Init(D3D12* d3d12) {
	modelCommon_ = new ModelCommon;
	modelCommon_->Init(d3d12);
}

//-----------------------------------------------------------------------------
// Purpose: シャットダウンします
//-----------------------------------------------------------------------------
void ModelManager::Shutdown() {
	delete instance_;
	instance_ = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: モデルをロードします
//-----------------------------------------------------------------------------
void ModelManager::LoadModel(const std::string& filePath) {
	// 読み込み済みモデルを検索
	if (models_.contains(filePath)) {
		// 読み込み済みなら早期return
		return;
	}

	// モデルの生成とファイルの読み込み、初期化
	std::unique_ptr<Model> model = std::make_unique<Model>();
	model->Init(modelCommon_, "./Resources/Models/", filePath);

	// モデルをmapコンテナに格納する
	models_.insert(std::make_pair(filePath, std::move(model)));
}

//-----------------------------------------------------------------------------
// Purpose: モデルを検索します
//-----------------------------------------------------------------------------
Model* ModelManager::FindModel(const std::string& filePath) const {
	// 読み込み済みモデルを検索
	if (models_.contains(filePath)) {
		// 読み込みモデルを戻り値としてreturn
		return models_.at(filePath).get();
	}

	// ファイル名一致なし
	return nullptr;
}
