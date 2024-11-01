#include "Particle.h"

#include "ParticleCommon.h"

#include "../TextureManager/TextureManager.h"
#include "../Lib/Console/Console.h"
#include "../Lib/Utils/ClientProperties.h"
#include "../Renderer/D3D12.h"

//-----------------------------------------------------------------------------
// Purpose : デストラクタ
//-----------------------------------------------------------------------------
Particle::~Particle() = default;

//-----------------------------------------------------------------------------
// Purpose : パーティクルを初期化します
//-----------------------------------------------------------------------------
void Particle::Init(ParticleCommon* particleCommon, const std::string& textureFilePath) {
	this->particleCommon_ = particleCommon;
	this->textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);

	// 各トランスフォームに初期値を設定
	transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	uvTransform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	// 頂点リソースにデータを書き込む
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

	// インデックスバッファの作成
	indexBuffer_ = std::make_unique<IndexBuffer>(particleCommon_->GetD3D12()->GetDevice(), sizeof(indices), indices);

	// 頂点バッファの作成
	vertexBuffer_ = std::make_unique<VertexBuffer>(
		particleCommon_->GetD3D12()->GetDevice(),
		sizeof(Vertex) * kParticleVertexCount,
		sizeof(Vertex),
		vertices
	);

	// 定数バッファ
	materialResource_ = std::make_unique<ConstantBuffer>(particleCommon_->GetD3D12()->GetDevice(), sizeof(Material));
	materialData_ = materialResource_->GetPtr<Material>();
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->enableLighting = false;
	materialData_->uvTransform = Mat4::Identity();

	transformation_ = std::make_unique<ConstantBuffer>(particleCommon_->GetD3D12()->GetDevice(),
		sizeof(TransformationMatrix));
	transformationMatrixData_ = transformation_->GetPtr<TransformationMatrix>();
	transformationMatrixData_->wvp = Mat4::Identity();
	transformationMatrixData_->world = Mat4::Identity();

	Console::Print("パーティクルの初期化に成功しました。\n", kConsoleColorCompleted);
}

//-----------------------------------------------------------------------------
// Purpose : パーティクルの更新処理
//-----------------------------------------------------------------------------
void Particle::Update() {
	float left = -anchorPoint_.x; // アンカーを考慮した左
	float right = 1.0f - anchorPoint_.x; // アンカーを考慮した右
	float top = -anchorPoint_.y; // アンカーを考慮した上
	float bottom = 1.0f - anchorPoint_.y; // アンカーを考慮した下

	// 左右反転
	if (isFlipX_) {
		left = -left;
		right = -right;
	}
	// 上下反転
	if (isFlipY_) {
		top = -top;
		bottom = -bottom;
	}

	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(textureIndex_);
	float texLeft = textureLeftTop.x / static_cast<float>(metadata.width);
	float texRight = (textureLeftTop.x + textureSize.x) / static_cast<float>(metadata.width);
	float texTop = textureLeftTop.y / static_cast<float>(metadata.height);
	float texBottom = (textureLeftTop.y + textureSize.y) / static_cast<float>(metadata.height);

	vertices[0].texcoord = { texLeft, texBottom };
	vertices[1].texcoord = { texLeft, texTop };
	vertices[2].texcoord = { texRight, texBottom };
	vertices[4].texcoord = { texRight, texTop };

	vertices[0].position = { left, bottom, 0.0f, 1.0f }; // 左下
	vertices[1].position = { left, top, 0.0f, 1.0f }; // 左上
	vertices[2].position = { right, bottom, 0.0f, 1.0f }; // 右下
	vertices[4].position = { right, top, 0.0f, 1.0f }; // 右上

	vertexBuffer_->Update(vertices, kParticleVertexCount);
	indexBuffer_->Update(indices, kParticleVertexCount);

	// uvTransformから行列を作成
	Mat4 uvTransformMat = Mat4::Scale(uvTransform_.scale);
	uvTransformMat = uvTransformMat * Mat4::RotateZ(uvTransform_.rotate.z);
	uvTransformMat = uvTransformMat * Mat4::Translate(uvTransform_.translate);
	// 設定
	materialData_->uvTransform = uvTransformMat;

	// 各種行列を作成
	Mat4 worldMat = Mat4::Affine(transform_.scale, transform_.rotate, transform_.translate);
	Mat4 viewMat = Mat4::Identity();
	Mat4 projMat = Mat4::MakeOrthographicMat(0.0f, 0.0f, static_cast<float>(kClientWidth),
		static_cast<float>(kClientHeight), 0.0f, 100.0f);

	TransformationMatrix worldViewProjectionMatrixSprite = {
		worldMat * viewMat * projMat,
		worldMat,
	};

	*transformationMatrixData_ = worldViewProjectionMatrixSprite;
}

//-----------------------------------------------------------------------------
// Purpose : パーティクルの描画処理
//-----------------------------------------------------------------------------
void Particle::Draw() const {
	// ディスクリプタヒープの設定
	ID3D12DescriptorHeap* descriptorHeaps[] = { particleCommon_->GetD3D12()->GetSRVDescriptorHeap() };
	particleCommon_->GetD3D12()->GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// 頂点バッファの設定
	D3D12_VERTEX_BUFFER_VIEW vbView = vertexBuffer_->View();
	particleCommon_->GetD3D12()->GetCommandList()->IASetVertexBuffers(0, 1, &vbView);

	// 定数バッファの設定
	particleCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetAddress());
	particleCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformation_->GetAddress());

	// SRVを設定
	particleCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootDescriptorTable(
		2, TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_));

	// インデックスバッファの設定
	D3D12_INDEX_BUFFER_VIEW indexBufferView = indexBuffer_->View();
	particleCommon_->GetD3D12()->GetCommandList()->IASetIndexBuffer(&indexBufferView);

	// 描画
	particleCommon_->GetD3D12()->GetCommandList()->DrawIndexedInstanced(kParticleVertexCount, 1, 0, 0, 0);
}

void Particle::ChangeTexture(const std::string& textureFilePath) {
	// 新しくインデックスを取得
	this->textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);

	// SRVを更新
	//	particleCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_));
}

Vec3 Particle::GetPos() const {
	return transform_.translate;
}

Vec3 Particle::GetRot() const {
	return transform_.rotate;
}

Vec3 Particle::GetSize() const {
	return transform_.scale;
}

Vec4 Particle::GetColor() const {
	return materialData_->color;
}

void Particle::SetPos(const Vec3& newPos) {
	transform_.translate = newPos;
}

void Particle::SetRot(const Vec3& newRot) {
	transform_.rotate = newRot;
}

void Particle::SetSize(const Vec3& newSize) {
	transform_.scale = newSize;
}

void Particle::SetColor(const Vec4 color) const {
	materialData_->color = color;
}