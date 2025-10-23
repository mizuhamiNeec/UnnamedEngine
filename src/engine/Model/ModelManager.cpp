#include <engine/Model/Model.h>
#include <engine/Model/ModelCommon.h>
#include <engine/Model/ModelManager.h>
#include <engine/renderer/ConstantBuffer.h>
#include <engine/renderer/VertexBuffer.h>

ModelManager* ModelManager::mInstance = nullptr;

/// @brief ModelManagerのインスタンスを取得します
ModelManager* ModelManager::GetInstance() {
	if (mInstance == nullptr) {
		mInstance = new ModelManager;
	}
	return mInstance;
}

/// @brief モデルマネージャーを初期化します
/// @param d3d12 D3D12レンダラーへのポインタ
void ModelManager::Init(D3D12* d3d12) {
	mModelCommon = new ModelCommon;
	mModelCommon->Init(d3d12);
}

/// @brief モデルマネージャーをシャットダウンします
void ModelManager::Shutdown() {
	delete mInstance;
	mInstance = nullptr;
}

/// @brief モデルを読み込みます
/// @param filePath モデルファイルのパス
void ModelManager::LoadModel(const std::string& filePath) {
	// 読み込み済みモデルを検索
	if (mModels.contains(filePath)) {
		// 読み込み済みなら早期return
		return;
	}

	// モデルの生成とファイルの読み込み、初期化
	auto model = std::make_unique<Model>();
	model->Init(mModelCommon, "./content/core/models/", filePath);

	// モデルをmapコンテナに格納する
	mModels.insert(std::make_pair(filePath, std::move(model)));
}

/// @brief モデルを検索します
/// @param filePath モデルファイルのパス
/// @return 見つかったモデルへのポインタ、見つからなかった場合はnullptr
Model* ModelManager::FindModel(const std::string& filePath) const {
	// 読み込み済みモデルを検索
	if (mModels.contains(filePath)) {
		// 読み込みモデルを戻り値としてreturn
		return mModels.at(filePath).get();
	}

	// ファイル名一致なし
	return nullptr;
}
