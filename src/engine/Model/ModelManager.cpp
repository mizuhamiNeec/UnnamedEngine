#include <engine/Model/Model.h>
#include <engine/Model/ModelCommon.h>
#include <engine/Model/ModelManager.h>
#include <engine/renderer/ConstantBuffer.h>
#include <engine/renderer/VertexBuffer.h>

ModelManager* ModelManager::mInstance = nullptr;

// HACK : 要修正
// TODO : シングルトンは悪!!
ModelManager* ModelManager::GetInstance() {
	if (mInstance == nullptr)
	{
		mInstance = new ModelManager;
	}
	return mInstance;
}

//-----------------------------------------------------------------------------
// Purpose: 初期化します
//-----------------------------------------------------------------------------
void ModelManager::Init(D3D12* d3d12) {
	mModelCommon = new ModelCommon;
	mModelCommon->Init(d3d12);
}

//-----------------------------------------------------------------------------
// Purpose: シャットダウンします
//-----------------------------------------------------------------------------
void ModelManager::Shutdown() {
	delete mInstance;
	mInstance = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: モデルをロードします
//-----------------------------------------------------------------------------
void ModelManager::LoadModel(const std::string& filePath) {
	// 読み込み済みモデルを検索
	if (mModels.contains(filePath))
	{
		// 読み込み済みなら早期return
		return;
	}

	// モデルの生成とファイルの読み込み、初期化
	auto model = std::make_unique<Model>();
	model->Init(mModelCommon, "./content/core/models/", filePath);

	// モデルをmapコンテナに格納する
	mModels.insert(std::make_pair(filePath, std::move(model)));
}

//-----------------------------------------------------------------------------
// Purpose: モデルを検索します
//-----------------------------------------------------------------------------
Model* ModelManager::FindModel(const std::string& filePath) const {
	// 読み込み済みモデルを検索
	if (mModels.contains(filePath))
	{
		// 読み込みモデルを戻り値としてreturn
		return mModels.at(filePath).get();
	}

	// ファイル名一致なし
	return nullptr;
}
