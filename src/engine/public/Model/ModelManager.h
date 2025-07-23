#pragma once
#include <map>
#include <memory>
#include <string>

class D3D12;
class ModelCommon;
class Model;

class ModelManager {
public:
	static ModelManager* GetInstance();
	
	void                 Init(D3D12* d3d12);
	static void          Shutdown();

	void                 LoadModel(const std::string& filePath);
	[[nodiscard]] Model* FindModel(const std::string& filePath) const;

private:
	static ModelManager* mInstance;

	ModelCommon* mModelCommon = nullptr;

	// モデルデータ
	std::map<std::string, std::unique_ptr<Model>> mModels;

	ModelManager()                         = default;
	~ModelManager()                        = default;
	ModelManager(ModelManager&)            = delete;
	ModelManager& operator=(ModelManager&) = delete;
};
