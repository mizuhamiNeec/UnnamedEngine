#pragma once
#include <map>
#include <memory>
#include <string>

class D3D12;
class ModelCommon;
class Model;

class ModelManager {
public:
	// シングルトンインスタンスの取得
	static ModelManager* GetInstance();

	// 初期化
	void Init(D3D12* d3d12);
	// 終了
	static void Shutdown();

	void LoadModel(const std::string& filePath);
	Model* FindModel(const std::string& filePath) const;

private:
	static ModelManager* instance_;

	ModelCommon* modelCommon_ = nullptr;

	// モデルデータ
	std::map<std::string, std::unique_ptr<Model>> models_;

private:
	ModelManager() = default;
	~ModelManager() = default;
	ModelManager(ModelManager&) = delete;
	ModelManager& operator=(ModelManager&) = delete;
};

