
#include "engine/public/Sprite/Sprite.h"

#include "engine/public/Sprite/SpriteCommon.h"
#include "engine/public/OldConsole/Console.h"

#include "engine/public/renderer/D3D12.h"
#include "engine/public/Window/WindowManager.h"
//-----------------------------------------------------------------------------
// Purpose : デストラクタ
//-----------------------------------------------------------------------------
Sprite::~Sprite() = default;

//-----------------------------------------------------------------------------
// Purpose : スプライトを初期化します
//-----------------------------------------------------------------------------
void Sprite::Init(SpriteCommon*      spriteCommon,
				  const std::string& textureFilePath) {
	this->spriteCommon_    = spriteCommon;
	this->textureFilePath_ = textureFilePath;

	// 各トランスフォームに初期値を設定
	transform_ = {
		.scale = {1.0f, 1.0f, 1.0f},
		.rotate = {0.0f, 0.0f, 0.0f},
		.translate = {0.0f, 0.0f, 0.0f}
	};
	uvTransform_ = {
		.scale = {1.0f, 1.0f, 1.0f},
		.rotate = {0.0f, 0.0f, 0.0f},
		.translate = {0.0f, 0.0f, 0.0f}
	};

	// 頂点リソースにデータを書き込む	
	vertices_.emplace_back(
		Vec4(0.0f, 1.0f, 0.0f, 1.0f),
		Vec2(0.0f, 1.0f),
		Vec3(0.0f, 0.0f, -1.0f)
	); // 左下
	vertices_.emplace_back(
		Vec4(0.0f, 0.0f, 0.0f, 1.0f),
		Vec2(0.0f, 0.0f),
		Vec3(0.0f, 0.0f, -1.0f)
	); // 左上
	vertices_.emplace_back(
		Vec4(1.0f, 1.0f, 0.0f, 1.0f),
		Vec2(1.0f, 1.0f),
		Vec3(0.0f, 0.0f, -1.0f)
	); // 右下
	vertices_.emplace_back(
		Vec4(0.0f, 0.0f, 0.0f, 1.0f),
		Vec2(0.0f, 0.0f),
		Vec3(0.0f, 0.0f, -1.0f)
	); // 左上
	vertices_.emplace_back(
		Vec4(1.0f, 0.0f, 0.0f, 1.0f),
		Vec2(1.0f, 0.0f),
		Vec3(0.0f, 0.0f, -1.0f)
	); // 右上
	vertices_.emplace_back(
		Vec4(1.0f, 1.0f, 0.0f, 1.0f),
		Vec2(1.0f, 1.0f),
		Vec3(0.0f, 0.0f, -1.0f)
	); // 右下

	// インデックスバッファの作成
	indexBuffer_ = std::make_unique<IndexBuffer>(
		spriteCommon_->GetD3D12()->GetDevice(), sizeof(indices), indices);

	// 頂点バッファの作成
	vertexBuffer_ = std::make_unique<VertexBuffer<Vertex>>(
		spriteCommon_->GetD3D12()->GetDevice(),
		sizeof(Vertex) * kSpriteVertexCount, vertices_.data()
	);

	// 定数バッファ
	materialResource_ = std::make_unique<ConstantBuffer>(
		spriteCommon_->GetD3D12()->GetDevice(), sizeof(Material),
		"SpriteMaterial");
	materialData_                 = materialResource_->GetPtr<Material>();
	materialData_->color          = {1.0f, 1.0f, 1.0f, 1.0f};
	materialData_->enableLighting = false;
	materialData_->uvTransform    = Mat4::identity;

	transformation_ = std::make_unique<ConstantBuffer>(
		spriteCommon_->GetD3D12()->GetDevice(), sizeof(TransformationMatrix),
		"SpriteTransformation"
	);
	transformationMatrixData_ = transformation_->GetPtr<TransformationMatrix>();
	transformationMatrixData_->wvp = Mat4::identity;
	transformationMatrixData_->world = Mat4::identity;

	AdjustTextureSize();

	Console::Print("スプライトの初期化に成功しました。\n", kConTextColorCompleted,
				   Channel::Engine);
}

//-----------------------------------------------------------------------------
// Purpose : スプライトの更新処理
//-----------------------------------------------------------------------------
void Sprite::Update() {
	float left   = -anchorPoint_.x;
	float right  = 1.0f - anchorPoint_.x;
	float top    = -anchorPoint_.y;
	float bottom = 1.0f - anchorPoint_.y;

	// 左右反転
	if (isFlipX_) {
		left  = -left;
		right = -right;
	}
	// 上下反転
	if (isFlipY_) {
		top    = -top;
		bottom = -bottom;
	}

	// すべての頂点を更新
	vertices_[0].position = Vec4(left, bottom, 0.0f, 1.0f);  // 左下
	vertices_[1].position = Vec4(left, top, 0.0f, 1.0f);     // 左上
	vertices_[2].position = Vec4(right, bottom, 0.0f, 1.0f); // 右下
	vertices_[3].position = Vec4(left, top, 0.0f, 1.0f);     // 左上
	vertices_[4].position = Vec4(right, top, 0.0f, 1.0f);    // 右上
	vertices_[5].position = Vec4(right, bottom, 0.0f, 1.0f); // 右下 (追加)

	//// すべてのUV座標を更新
	//vertices_[0].uv = { texLeft, texBottom };   // 左下
	//vertices_[1].uv = { texLeft, texTop };      // 左上
	//vertices_[2].uv = { texRight, texBottom };  // 右下
	//vertices_[3].uv = { texLeft, texTop };      // 左上
	//vertices_[4].uv = { texRight, texTop };     // 右上
	//vertices_[5].uv = { texRight, texBottom };  // 右下 (追加)
}

//-----------------------------------------------------------------------------
// Purpose : スプライトの描画処理
//-----------------------------------------------------------------------------
void Sprite::Draw() const {
	{
		vertexBuffer_->Update(vertices_.data(),
							  sizeof(Vertex) * vertices_.size());
		indexBuffer_->Update(indices, sizeof(uint16_t) * kSpriteVertexCount);

		// uvTransformから行列を作成
		Mat4 uvTransformMat = Mat4::Scale(uvTransform_.scale);
		uvTransformMat = uvTransformMat * Mat4::RotateZ(uvTransform_.rotate.z);
		uvTransformMat = uvTransformMat * Mat4::Translate(
			uvTransform_.translate);
		// 設定
		materialData_->uvTransform = uvTransformMat;

		// 各種行列を作成
		const Mat4 worldMat = Mat4::Affine(transform_.scale, transform_.rotate,
										   transform_.translate);
		const Mat4 viewMat = Mat4::identity;
		const Mat4 projMat = Mat4::MakeOrthographicMat(
			0.0f,
			0.0f,
			static_cast<float>(OldWindowManager::GetMainWindow()->
				GetClientWidth()),
			static_cast<float>(OldWindowManager::GetMainWindow()->
				GetClientHeight()),
			0.0f,
			100.0f
		);

		const TransformationMatrix worldViewProjectionMatrixSprite = {
			worldMat * viewMat * projMat,
			worldMat,
		};

		*transformationMatrixData_ = worldViewProjectionMatrixSprite;
	}

	// 頂点バッファの設定
	const D3D12_VERTEX_BUFFER_VIEW vbView = vertexBuffer_->View();
	spriteCommon_->GetD3D12()->GetCommandList()->IASetVertexBuffers(
		0, 1, &vbView);

	// 定数バッファの設定
	spriteCommon_->GetD3D12()->GetCommandList()->
				   SetGraphicsRootConstantBufferView(
					   0, materialResource_->GetAddress());
	spriteCommon_->GetD3D12()->GetCommandList()->
				   SetGraphicsRootConstantBufferView(
					   1, transformation_->GetAddress());

	// インデックスバッファの設定
	const D3D12_INDEX_BUFFER_VIEW indexBufferView = indexBuffer_->View();
	spriteCommon_->GetD3D12()->GetCommandList()->IASetIndexBuffer(
		&indexBufferView);

	// 描画
	spriteCommon_->GetD3D12()->GetCommandList()->DrawIndexedInstanced(
		6, 1, 0, 0, 0);
}

void Sprite::ChangeTexture(const std::string& textureFilePath) {
	// テクスチャのパスを変更
	this->textureFilePath_ = textureFilePath;
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

Vec2 Sprite::GetAnchorPoint() const {
	return anchorPoint_;
}

Vec4 Sprite::GetColor() const {
	return materialData_->color;
}

Vec2 Sprite::GetTextureLeftTop() const {
	return textureLeftTop;
}

Vec2 Sprite::GetTextureSize() const {
	return textureSize;
}

bool Sprite::GetIsFlipX() const {
	return isFlipX_;
}

bool Sprite::GetIsFlipY() const {
	return isFlipY_;
}

Vec2 Sprite::GetUvPos() {
	return {uvTransform_.translate.x, uvTransform_.translate.y};
}

Vec2 Sprite::GetUvSize() {
	return {uvTransform_.scale.x, uvTransform_.scale.y};
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

void Sprite::SetAnchorPoint(const Vec2& anchorPoint) {
	this->anchorPoint_ = anchorPoint;
}

void Sprite::SetColor(const Vec4 color) const {
	materialData_->color = color;
}

void Sprite::SetIsFlipX(const bool isFlipX) {
	isFlipX_ = isFlipX;
}

void Sprite::SetIsFlipY(const bool isFlipY) {
	isFlipY_ = isFlipY;
}

void Sprite::SetTextureLeftTop(const Vec2& newTextureLeftTop) {
	this->textureLeftTop = newTextureLeftTop;
}

void Sprite::SetTextureSize(const Vec2& newTextureSize) {
	this->textureSize = newTextureSize;
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

void Sprite::AdjustTextureSize() {
	transform_.scale.x = textureSize.x;
	transform_.scale.y = textureSize.y;
}
