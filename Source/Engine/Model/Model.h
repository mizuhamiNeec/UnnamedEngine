#pragma once
#include <memory>
#include <vector>

#include "../Lib/Structs/Structs.h"

#include "../Renderer/VertexBuffer.h"

class ConstantBuffer;
class ModelCommon;

class Model {
	struct ModelData {
		std::vector<Vertex> vertices;
		MaterialData material;
	};

public:
	void Init(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& fileName);
	void ImGuiDraw();
	void Draw() const;

	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

private:
	ModelCommon* modelCommon_ = nullptr;
	ModelData modelData_; // Objファイルのデータ
	Material* materialData_ = nullptr; // マテリアルのポインタ

	std::unique_ptr<VertexBuffer<Vertex>> vertexBuffer_;
	std::unique_ptr<ConstantBuffer> materialConstantBuffer_;
};
