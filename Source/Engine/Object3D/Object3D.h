#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../Lib/Math/Matrix/Mat4.h"
#include "../Lib/Math/Vector/Vec4.h"
#include "../Lib/Structs/Structs.h"

class ConstantBuffer;
struct Vertex;
class VertexBuffer;
class Object3DCommon;

class Object3D {
	struct MaterialData {
		std::string textureFilePath;
		uint32_t textureIndex = 0;
	};

	struct ModelData {
		std::vector<Vertex> vertices;
		MaterialData material;
	};

	// マテリアル
	struct Material {
		Vec4 color;
		int32_t enableLighting;
		float padding[3];
		Mat4 uvTransform;
	};

	// 座標変換行列データ
	struct TransformationMatrix {
		Mat4 wvp;
		Mat4 world;
	};

	// 指向性ライト
	struct DirectionalLight {
		Vec4 color;
		Vec3 direction;
		float intensity;
	};

public:
	void Initialize(Object3DCommon* object3DCommon);
	void Update() const;
	void Draw() const;

	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);
private:
	Object3DCommon* object3DCommon_ = nullptr;

	Transform transform_;
	Transform cameraTransform_;

	ModelData modelData_; // Objファイルのデータ
	Material* materialData_ = nullptr; // マテリアルのポインタ
	TransformationMatrix* transformationMatrixData_ = nullptr; // 座標変換行列のポインタ
	DirectionalLight* directionalLightData_ = nullptr; // 指向性ライトのポインタ

	std::unique_ptr<VertexBuffer> vertexBuffer_;
	std::unique_ptr<ConstantBuffer> materialConstantBuffer_;
	std::unique_ptr<ConstantBuffer> transformationMatrixConstantBuffer_;
	std::unique_ptr<ConstantBuffer> directionalLightConstantBuffer_;
};

