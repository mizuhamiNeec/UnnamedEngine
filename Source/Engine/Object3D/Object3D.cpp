#include "Object3D.h"

#include <cassert>
#include <format>
#include <fstream>
#include <sstream>

#include "Object3DCommon.h"
#include "../Lib/Math/Vector/Vec2.h"
#include "../Lib/Math/Vector/Vec3.h"
#include "../Lib/Math/Vector/Vec4.h"
#include "../Lib/Structs/Structs.h"
#include "../Renderer/VertexBuffer.h"
#include "../Renderer/ConstantBuffer.h"
#include "../Lib/Console/Console.h"
#include "../Lib/Math/MathLib.h"
#include "../TextureManager/TextureManager.h"

//-----------------------------------------------------------------------------
// Purpose : 初期化します
//-----------------------------------------------------------------------------
void Object3D::Initialize(Object3DCommon* object3DCommon) {
	// 引数で受け取ってメンバ変数に記録する
	this->object3DCommon_ = object3DCommon;

	modelData_ = LoadObjFile("Resources/Models", "suzanne.obj");

	// 頂点バッファ modelData_を突っ込む
	vertexBuffer_ = std::make_unique<VertexBuffer>(object3DCommon_->GetD3D12()->GetDevice(), sizeof(Vertex) * modelData_.vertices.size(), sizeof(Vertex), modelData_.vertices.data());

	// マテリアル定数バッファ
	materialConstantBuffer_ = std::make_unique<ConstantBuffer>(object3DCommon_->GetD3D12()->GetDevice(), sizeof(Material));
	materialData_ = materialConstantBuffer_->GetPtr<Material>();
	materialData_->color = { 1.0f,1.0f,1.0f,1.0f }; // 白
	materialData_->enableLighting = false;
	materialData_->uvTransform = Mat4::Identity();

	// 座標変換行列定数バッファ
	transformationMatrixConstantBuffer_ = std::make_unique<ConstantBuffer>(object3DCommon_->GetD3D12()->GetDevice(), sizeof(TransformationMatrix));
	transformationMatrixData_ = transformationMatrixConstantBuffer_->GetPtr<TransformationMatrix>();
	transformationMatrixData_->wvp = Mat4::Identity();
	transformationMatrixData_->world = Mat4::Identity();

	// 指向性ライト定数バッファ
	directionalLightConstantBuffer_ = std::make_unique<ConstantBuffer>(object3DCommon_->GetD3D12()->GetDevice(), sizeof(DirectionalLight));
	directionalLightData_ = directionalLightConstantBuffer_->GetPtr<DirectionalLight>();
	directionalLightData_->color = { 1.0f,1.0f,1.0f,1.0f }; // 白
	directionalLightData_->direction = { 0.0f,0.7071067812f,0.7071067812f }; // 斜め前向き
	directionalLightData_->intensity = 1.0f; // 明るさ1

	// テクスチャのファイルパスが空ではなかったら
	if (!modelData_.material.textureFilePath.empty()) {
		// .objの参照しているテクスチャファイル読み込み
		TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);
		// 読み込んだテクスチャの番号を取得
		modelData_.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData_.material.textureFilePath);
	}

	// 各トランスフォームの初期化
	transform_ = { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
	cameraTransform_ = { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
}

void Object3D::Update() const {
	Mat4 worldMat = Mat4::Affine(transform_.scale, transform_.rotate, transform_.translate);
	Mat4 cameraMat = Mat4::Affine(cameraTransform_.scale, cameraTransform_.rotate, cameraTransform_.translate);
	Mat4 viewMat = cameraMat.Inverse();
	Mat4 projMat = Mat4::PerspectiveFovMat(
		90.0f * Math::deg2Rad,
		static_cast<float>(object3DCommon_->GetD3D12()->GetWindow()->GetClientWidth()) / static_cast<float>(object3DCommon_->GetD3D12()->GetWindow()->GetClientHeight()),
		0.01f,
		1000.0f
	);
	transformationMatrixData_->wvp = worldMat * viewMat * projMat;
	transformationMatrixData_->world = worldMat;
}

void Object3D::Draw() const {
	// VertexBufferViewを設定
	D3D12_VERTEX_BUFFER_VIEW vbView = vertexBuffer_->View();
	object3DCommon_->GetD3D12()->GetCommandList()->IASetVertexBuffers(0, 1, &vbView);

	// マテリアルの定数バッファの設定
	object3DCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialConstantBuffer_->GetAddress());

	// 座標変換行列の定数バッファの設定
	object3DCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixConstantBuffer_->GetAddress());

	// SRVのDescriptorTableの先頭を設定
	object3DCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(modelData_.material.textureIndex));

	// 指向性ライトの定数バッファを設定
	object3DCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightConstantBuffer_->GetAddress());

	// 描画
	object3DCommon_->GetD3D12()->GetCommandList()->DrawInstanced(static_cast<UINT>(modelData_.vertices.size()), 1, 0, 0);
}

Object3D::MaterialData Object3D::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	MaterialData materialData; // 構築するMaterialData
	std::string line; // ファイルから読んだ1行を格納するもの
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず開けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// identifierに応じた処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			// 連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}

	return materialData;
}

Object3D::ModelData Object3D::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData; // 構築するModelData
	std::vector<Vec4> positions; // 位置
	std::vector<Vec3> normals; // 法線
	std::vector<Vec2> texcoords; // テクスチャの座標
	std::string line;

	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open());

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; // 先頭の識別子を読む

		if (identifier == "v") {
			Vec4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);
		} else if (identifier == "vt") {
			Vec2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn") {
			Vec3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);
		} else if (identifier == "f") {
			Vertex triangle[3];

			// 面は三角形限定。その他は未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;
				// 頂点の要素へのIndexは[位置/UV/法線] で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				std::string index;
				uint32_t elementIndices[3] = { 0, 0, 0 };
				int element = 0;

				while (std::getline(v, index, '/') && element < 3) {
					if (!index.empty()) {
						elementIndices[element] = std::stoi(index);
					}
					++element;
				}

				// 要素へのIndexから、実際の要素の値を取得して、頂点を構築する
				Vec4 position = positions[elementIndices[0] - 1];
				Vec2 texcoord = elementIndices[1] ? texcoords[elementIndices[1] - 1] : Vec2{ 0.0f, 0.0f };
				Vec3 normal = elementIndices[2] ? normals[elementIndices[2] - 1] : Vec3{ 0.0f, 0.0f, 0.0f };

				position.x *= -1.0f;
				normal.x *= -1.0f;
				triangle[faceVertex] = { position, texcoord, normal };
			}
			// 頂点を逆順で登録することで、周り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		} else if (identifier == "mtllib") {
			// materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			// 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}

	return modelData;
}
