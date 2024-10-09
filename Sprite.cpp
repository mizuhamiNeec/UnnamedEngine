#include "Sprite.h"

#include "Console.h"
#include "SpriteCommon.h"
#include "Source/Engine/Lib/Structs/Structs.h"
#include "Source/Engine/Renderer/VertexBuffer.h"
#include "Source/Engine/TextureManager/Texture.h"
#include "Source/Engine/TextureManager/TextureManager.h"

std::shared_ptr<Texture> debug;

// スプライトの頂点数を定義
constexpr uint32_t kSpriteVertexCount = 6;

Sprite::~Sprite() {
	debug.reset();
}

void Sprite::Init(SpriteCommon* spriteCommon) {
	this->spriteCommon_ = spriteCommon;

	// 各トランスフォームに初期値を設定
	transform_ = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
	uvTransform_ = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };

	Vertex vertices[kSpriteVertexCount] = {};
	// 1枚目の三角形
	vertices[0].position = { 0.0f, 360.0f, 0.0f, 1.0f }; // 左下
	vertices[0].texcoord = { 0.0f, 1.0f };
	vertices[0].normal = { 0.0f, 0.0f, -1.0f };
	vertices[1].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左上
	vertices[1].texcoord = { 0.0f, 0.0f };
	vertices[1].normal = { 0.0f, 0.0f, -1.0f };
	vertices[2].position = { 640.0f, 360.0f, 0.0f, 1.0f }; // 右下
	vertices[2].texcoord = { 1.0f, 1.0f };
	vertices[2].normal = { 0.0f, 0.0f, -1.0f };
	// 2枚目の三角形
	vertices[3].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左上
	vertices[3].texcoord = { 0.0f, 0.0f };
	vertices[3].normal = { 0.0f, 0.0f, -1.0f };
	vertices[4].position = { 640.0f, 0.0f, 0.0f, 1.0f }; // 右上
	vertices[4].texcoord = { 1.0f, 0.0f };
	vertices[4].normal = { 0.0f, 0.0f, -1.0f };
	vertices[5].position = { 640.0f, 360.0f, 0.0f, 1.0f }; // 右下
	vertices[5].texcoord = { 1.0f, 1.0f };
	vertices[5].normal = { 0.0f, 0.0f, -1.0f };

	// インデックスの作成
	uint16_t indices[] = {
		0, 1, 2, // 1枚目の三角形
		2, 1, 4  // 2枚目の三角形
	};
	// インデックスバッファの作成
	indexBuffer_ = std::make_unique<IndexBuffer>(spriteCommon_->GetD3D12()->GetDevice(), sizeof(indices), indices);
	if (indexBuffer_) {
		Console::Print("インデックスバッファの生成に成功.\n");
	}

	size_t vertexStride = sizeof(Vertex);
	vertexBuffer_ = std::make_unique<VertexBuffer>(spriteCommon_->GetD3D12()->GetDevice(), sizeof(Vertex) * kSpriteVertexCount, vertexStride, vertices);
	if (vertexBuffer_) {
		Console::Print("頂点バッファの生成に成功.\n");
	}

	// 定数バッファ
	materialResource_ = std::make_unique<ConstantBuffer>(spriteCommon_->GetD3D12()->GetDevice(), sizeof(Material));
	materialData_ = materialResource_->GetPtr<Material>();
	materialData_->color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = false;
	materialData_->uvTransform = Mat4::Identity();

	transformation_ = std::make_unique<ConstantBuffer>(spriteCommon_->GetD3D12()->GetDevice(), sizeof(TransformationMatrix));
	transformationMatrixData_ = transformation_->GetPtr<TransformationMatrix>();
	transformationMatrixData_->WVP = Mat4::Identity();
	transformationMatrixData_->World = Mat4::Identity();

	debug = TextureManager::GetInstance().GetTexture(spriteCommon_->GetD3D12()->GetDevice(), L"./Resources/Textures/uvChecker.png");
}

void Sprite::Update() {
	ImGui::Begin("Sprite");
	if (ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("pos##sprite", &transform_.translate.x, 0.01f);
		ImGui::DragFloat3("rot##sprite", &transform_.rotate.x, 0.01f);
		ImGui::DragFloat3("scale##sprite", &transform_.scale.x, 0.01f);

		ImGui::Separator();

		if (ImGui::CollapsingHeader("UV", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat2("pos##uv", &uvTransform_.translate.x, 0.01f);
			ImGui::DragFloat2("scale##uv", &uvTransform_.scale.x, 0.01f);
			ImGui::SliderAngle("rotZ##uv", &uvTransform_.rotate.z);
		}
	}

	ImGui::End();

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

void Sprite::Draw() {
	// ディスクリプタヒープの設定
	ID3D12DescriptorHeap* descriptorHeaps[] = { debug->GetSRVHeap() };
	spriteCommon_->GetD3D12()->GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// 頂点バッファの設定
	D3D12_VERTEX_BUFFER_VIEW vbView = vertexBuffer_->View();
	spriteCommon_->GetD3D12()->GetCommandList()->IASetVertexBuffers(0, 1, &vbView);

	// 定数バッファの設定
	spriteCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetAddress());
	spriteCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformation_->GetAddress());

	// SRVを設定
	spriteCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootDescriptorTable(2, debug->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart());

	// インデックスバッファの設定
	D3D12_INDEX_BUFFER_VIEW indexBufferView = indexBuffer_->View();
	spriteCommon_->GetD3D12()->GetCommandList()->IASetIndexBuffer(&indexBufferView);

	// 描画
	spriteCommon_->GetD3D12()->GetCommandList()->DrawIndexedInstanced(kSpriteVertexCount, 1, 0, 0, 0);
}

void Sprite::SetTransform(const Transform& newTransform) {
	transform_ = newTransform;
}

void Sprite::SetPos(const Vec3& newPos) {
	transform_.translate = newPos;
}

void Sprite::SetRot(const Vec3& newRot) {
	transform_.rotate = newRot;
}

void Sprite::SetScale(const Vec3& newScale) {
	transform_.scale = newScale;
}