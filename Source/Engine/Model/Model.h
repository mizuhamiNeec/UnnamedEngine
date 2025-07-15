#pragma once
#include <memory>
#include <vector>

#include <Lib/Structs/Structs.h>

#include <Renderer/VertexBuffer.h>

class ConstantBuffer;
class ModelCommon;

class Model {
	struct ModelData {
		std::vector<Vertex> vertices;
		MaterialData        material;
	};

public:
	void Init(ModelCommon*       modelCommon, const std::string& directoryPath,
	          const std::string& fileName);
	void ImGuiDraw() const;
	void Draw() const;

	static MaterialData LoadMaterialTemplateFile(
		const std::string& directoryPath, const std::string& filename);
	static ModelData LoadObjFile(const std::string& directoryPath,
	                             const std::string& filename);

private:
	struct Material {
		Vec4    color;
		int32_t enableLighting;
		Vec3    padding;
		Mat4    uvTransform;
		float   shininess;
		Vec3    specularColor;
	};

	ModelCommon* mModelCommon = nullptr;
	ModelData    mModelData;              // Objファイルのデータ
	Material*    mMaterialData = nullptr; // マテリアルのポインタ

	std::unique_ptr<VertexBuffer<Vertex>> mVertexBuffer;
	std::unique_ptr<ConstantBuffer>       mMaterialConstantBuffer;
};
