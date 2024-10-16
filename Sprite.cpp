#include "Sprite.h"

#include "Console.h"
#include "SpriteCommon.h"
#include "Source/Engine/Lib/Structs/Structs.h"
#include "Source/Engine/Renderer/VertexBuffer.h"
#include "Source/Engine/TextureManager/TextureManager.h"

// スプライトの頂点数を定義
constexpr uint32_t kSpriteVertexCount = 6;

//-----------------------------------------------------------------------------
// Purpose : デストラクタ
//-----------------------------------------------------------------------------
Sprite::~Sprite() {}

//-----------------------------------------------------------------------------
// Purpose : スプライトを初期化します
//-----------------------------------------------------------------------------
void Sprite::Init(SpriteCommon* spriteCommon, const std::string& textureFilePath) {
	this->spriteCommon_ = spriteCommon;
	this->textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);

	// 各トランスフォームに初期値を設定
	transform_ = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
	uvTransform_ = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };

	Vertex vertices[kSpriteVertexCount] = {};
	// 1枚目の三角形
	vertices[0].position = { 0.0f, 1.0f, 0.0f, 1.0f }; // 左下
	vertices[0].texcoord = { 0.0f, 1.0f };
	vertices[0].normal = { 0.0f, 0.0f, -1.0f };
	vertices[1].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左上
	vertices[1].texcoord = { 0.0f, 0.0f };
	vertices[1].normal = { 0.0f, 0.0f, -1.0f };
	vertices[2].position = { 1.0f, 1.0f, 0.0f, 1.0f }; // 右下
	vertices[2].texcoord = { 1.0f, 1.0f };
	vertices[2].normal = { 0.0f, 0.0f, -1.0f };
	// 2枚目の三角形
	vertices[3].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左上
	vertices[3].texcoord = { 0.0f, 0.0f };
	vertices[3].normal = { 0.0f, 0.0f, -1.0f };
	vertices[4].position = { 1.0f, 0.0f, 0.0f, 1.0f }; // 右上
	vertices[4].texcoord = { 1.0f, 0.0f };
	vertices[4].normal = { 0.0f, 0.0f, -1.0f };
	vertices[5].position = { 1.0f, 1.0f, 0.0f, 1.0f }; // 右下
	vertices[5].texcoord = { 1.0f, 1.0f };
	vertices[5].normal = { 0.0f, 0.0f, -1.0f };

	// インデックスの作成
	uint16_t indices[] = {
		0, 1, 2, // 1枚目の三角形
		2, 1, 4  // 2枚目の三角形
	};
	// インデックスバッファの作成
	indexBuffer_ = std::make_unique<IndexBuffer>(spriteCommon_->GetD3D12()->GetDevice(), sizeof(indices), indices);

	// 頂点バッファの作成
	size_t vertexStride = sizeof(Vertex);
	vertexBuffer_ = std::make_unique<VertexBuffer>(spriteCommon_->GetD3D12()->GetDevice(), sizeof(Vertex) * kSpriteVertexCount, vertexStride, vertices);

	// 定数バッファ
	materialResource_ = std::make_unique<ConstantBuffer>(spriteCommon_->GetD3D12()->GetDevice(), sizeof(Material));
	materialData_ = materialResource_->GetPtr<Material>();
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->enableLighting = false;
	materialData_->uvTransform = Mat4::Identity();

	transformation_ = std::make_unique<ConstantBuffer>(spriteCommon_->GetD3D12()->GetDevice(), sizeof(TransformationMatrix));
	transformationMatrixData_ = transformation_->GetPtr<TransformationMatrix>();
	transformationMatrixData_->wvp = Mat4::Identity();
	transformationMatrixData_->world = Mat4::Identity();

	Console::Print("スプライトの初期化に成功しました。\n");
}

//-----------------------------------------------------------------------------
// Purpose : スプライトの更新処理
//-----------------------------------------------------------------------------
void Sprite::Update() const {
	// uvTransformから行列を作成
	Mat4 uvTransformMat = Mat4::Scale(uvTransform_.scale);
	uvTransformMat = uvTransformMat * Mat4::RotateZ(uvTransform_.rotate.z);
	uvTransformMat = uvTransformMat * Mat4::Translate(uvTransform_.translate);
	// 設定
	materialData_->uvTransform = uvTransformMat;

	// 各種行列を作成
	Mat4 worldMat = Mat4::Affine(transform_.scale, transform_.rotate, transform_.translate);
	Mat4 viewMat = Mat4::Identity();
	Mat4 projMat = Mat4::MakeOrthographicMat(0.0f, 0.0f, static_cast<float>(kClientWidth), static_cast<float>(kClientHeight), 0.0f, 100.0f);

	TransformationMatrix worldViewProjectionMatrixSprite = {
		worldMat * viewMat * projMat,
		worldMat,
	};

	*transformationMatrixData_ = worldViewProjectionMatrixSprite;
}

//-----------------------------------------------------------------------------
// Purpose : スプライトの描画処理
//-----------------------------------------------------------------------------
void Sprite::Draw() const {
	//// ディスクリプタヒープの設定
	ID3D12DescriptorHeap* descriptorHeaps[] = { spriteCommon_->GetD3D12()->GetSRVDescriptorHeap() };
	spriteCommon_->GetD3D12()->GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// 頂点バッファの設定
	D3D12_VERTEX_BUFFER_VIEW vbView = vertexBuffer_->View();
	spriteCommon_->GetD3D12()->GetCommandList()->IASetVertexBuffers(0, 1, &vbView);

	// 定数バッファの設定
	spriteCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetAddress());
	spriteCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformation_->GetAddress());

	// SRVを設定
	spriteCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_));

	// インデックスバッファの設定
	D3D12_INDEX_BUFFER_VIEW indexBufferView = indexBuffer_->View();
	spriteCommon_->GetD3D12()->GetCommandList()->IASetIndexBuffer(&indexBufferView);

	// 描画
	spriteCommon_->GetD3D12()->GetCommandList()->DrawIndexedInstanced(kSpriteVertexCount, 1, 0, 0, 0);
}

void Sprite::ChangeTexture(const std::string& textureFilePath) {
	// 新しくインデックスを取得
	this->textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);

	// SRVを更新
//	spriteCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_));
}

Vec3 Sprite::GetPos() const {
	return transform_.translate;
}

Vec3 Sprite::GetRot() const {
	return transform_.rotate;
}

Vec3 Sprite::GetSize() const {
	return transform_.scale;
}

Vec4 Sprite::GetColor() const {
	return materialData_->color;
}

Vec2 Sprite::GetUvPos() {
	return { uvTransform_.translate.x,uvTransform_.translate.y };
}

Vec2 Sprite::GetUvSize() {
	return { uvTransform_.scale.x,uvTransform_.scale.y };
}

float Sprite::GetUvRot() const {
	return uvTransform_.rotate.z;
}

void Sprite::SetPos(const Vec3& newPos) {
	transform_.translate = newPos;
}

void Sprite::SetRot(const Vec3& newRot) {
	transform_.rotate = newRot;
}

void Sprite::SetSize(const Vec3& newSize) {
	transform_.scale = newSize;
}

void Sprite::SetColor(const Vec4 color) const {
	materialData_->color = color;
}

void Sprite::SetUvPos(const Vec2& newPos) {
	for (uint32_t i = 0; i < 2; ++i) {
		uvTransform_.translate[i] = newPos[i];
	}
}

void Sprite::SetUvSize(const Vec2& newSize) {
	for (uint32_t i = 0; i < 2; ++i) {
		uvTransform_.scale[i] = newSize[i];
	}
}

void Sprite::SetUvRot(const float& newRot) {
	uvTransform_.rotate.z = newRot;
}
