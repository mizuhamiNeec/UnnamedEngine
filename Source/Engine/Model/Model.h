#pragma once
#include <memory>
#include <vector>

#include "../Lib/Structs/Structs.h"

class ConstantBuffer;
class VertexBuffer;
class ModelCommon;

class Model {
	struct MaterialData {
		std::string textureFilePath;
		uint32_t textureIndex = 0;
	};

	struct ModelData {
		std::vector<Vertex> vertices;
		MaterialData material;
	};
public:
	void Init(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& fileName);
	void Draw() const;

	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

private:
	ModelCommon* modelCommon_ = nullptr;
	ModelData modelData_; // Objファイルのデータ
	Material* materialData_ = nullptr; // マテリアルのポインタ

	std::unique_ptr<VertexBuffer> vertexBuffer_;
	std::unique_ptr<ConstantBuffer> materialConstantBuffer_;
};

