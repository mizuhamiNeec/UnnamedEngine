#include "engine/Sprite/Sprite.h"

#include <algorithm>

#include "engine/OldConsole/Console.h"
#include "engine/Sprite/SpriteCommon.h"

#include "engine/renderer/D3D12.h"
#include "engine/Engine.h"
#include "engine/EngineServices.h"
#include "engine/TextureManager/TexManager.h"
#include "engine/Window/WindowManager.h"

/// @brief スプライトのインデックスデータ
Sprite::~Sprite() = default;

/// @brief スプライトの初期化
/// @param spriteCommon スプライト共通情報へのポインタ
/// @param textureFilePath テクスチャファイルパス
void Sprite::Init(
	SpriteCommon*      spriteCommon,
	const std::string& textureFilePath
) {
	this->mSpriteCommon    = spriteCommon;
	this->mTextureFilePath = textureFilePath;

	// 各トランスフォームに初期値を設定
	mTransform = {
		.scale     = {1.0f, 1.0f, 1.0f},
		.rotate    = {0.0f, 0.0f, 0.0f},
		.translate = {0.0f, 0.0f, 0.0f}
	};
	mUvTransform = {
		.scale     = {1.0f, 1.0f, 1.0f},
		.rotate    = {0.0f, 0.0f, 0.0f},
		.translate = {0.0f, 0.0f, 0.0f}
	};

	// 頂点リソースにデータを書き込む	
	mVertices.emplace_back(
		Vec4(0.0f, 1.0f, 0.0f, 1.0f),
		Vec2(0.0f, 1.0f),
		Vec3(0.0f, 0.0f, -1.0f)
	); // 左下
	mVertices.emplace_back(
		Vec4(0.0f, 0.0f, 0.0f, 1.0f),
		Vec2(0.0f, 0.0f),
		Vec3(0.0f, 0.0f, -1.0f)
	); // 左上
	mVertices.emplace_back(
		Vec4(1.0f, 1.0f, 0.0f, 1.0f),
		Vec2(1.0f, 1.0f),
		Vec3(0.0f, 0.0f, -1.0f)
	); // 右下
	mVertices.emplace_back(
		Vec4(0.0f, 0.0f, 0.0f, 1.0f),
		Vec2(0.0f, 0.0f),
		Vec3(0.0f, 0.0f, -1.0f)
	); // 左上
	mVertices.emplace_back(
		Vec4(1.0f, 0.0f, 0.0f, 1.0f),
		Vec2(1.0f, 0.0f),
		Vec3(0.0f, 0.0f, -1.0f)
	); // 右上
	mVertices.emplace_back(
		Vec4(1.0f, 1.0f, 0.0f, 1.0f),
		Vec2(1.0f, 1.0f),
		Vec3(0.0f, 0.0f, -1.0f)
	); // 右下

	// インデックスバッファの作成
	mIndexBuffer = std::make_unique<IndexBuffer>(
		mSpriteCommon->GetD3D12()->GetDevice(), sizeof(mIndices), mIndices
	);

	// 頂点バッファの作成
	mVertexBuffer = std::make_unique<VertexBuffer<Vertex>>(
		mSpriteCommon->GetD3D12()->GetDevice(),
		sizeof(Vertex) * kSpriteVertexCount, mVertices.data()
	);

	// 定数バッファ
	mMaterialResource = std::make_unique<ConstantBuffer>(
		mSpriteCommon->GetD3D12()->GetDevice(), sizeof(Material),
		"SpriteMaterial"
	);
	mMaterialData                 = mMaterialResource->GetPtr<Material>();
	mMaterialData->color          = {1.0f, 1.0f, 1.0f, 1.0f};
	mMaterialData->enableLighting = false;
	mMaterialData->uvTransform    = Mat4::identity;

	mTransformation = std::make_unique<ConstantBuffer>(
		mSpriteCommon->GetD3D12()->GetDevice(), sizeof(TransformationMatrix),
		"SpriteTransformation"
	);
	mTransformationMatrixData = mTransformation->GetPtr<TransformationMatrix>();
	mTransformationMatrixData->wvp = Mat4::identity;
	mTransformationMatrixData->world = Mat4::identity;

	AdjustTextureSize();

	Console::Print(
		"スプライトの初期化に成功しました。\n", kConTextColorCompleted,
		Channel::Engine
	);
}

/// @brief スプライトの頂点情報を更新
void Sprite::Update() {
	float left   = -mAnchorPoint.x;
	float right  = 1.0f - mAnchorPoint.x;
	float top    = -mAnchorPoint.y;
	float bottom = 1.0f - mAnchorPoint.y;

	const float textureWidth = std::max(1.0f, mTextureResourceSize.x);
	const float textureHeight = std::max(1.0f, mTextureResourceSize.y);
	float texLeft = mTextureLeftTop.x / textureWidth;
	float texTop = mTextureLeftTop.y / textureHeight;
	float texRight = (mTextureLeftTop.x + mTextureSize.x) / textureWidth;
	float texBottom = (mTextureLeftTop.y + mTextureSize.y) / textureHeight;

	// 左右反転
	if (mIsFlipX) {
		left  = -left;
		right = -right;
		std::swap(texLeft, texRight);
	}
	// 上下反転
	if (mIsFlipY) {
		top    = -top;
		bottom = -bottom;
		std::swap(texTop, texBottom);
	}

	// すべての頂点を更新
	mVertices[0].position = Vec4(left, bottom, 0.0f, 1.0f);  // 左下
	mVertices[1].position = Vec4(left, top, 0.0f, 1.0f);     // 左上
	mVertices[2].position = Vec4(right, bottom, 0.0f, 1.0f); // 右下
	mVertices[3].position = Vec4(left, top, 0.0f, 1.0f);     // 左上
	mVertices[4].position = Vec4(right, top, 0.0f, 1.0f);    // 右上
	mVertices[5].position = Vec4(right, bottom, 0.0f, 1.0f); // 右下 (追加)

	// すべてのUV座標を更新
	mVertices[0].uv = {texLeft, texBottom};  // 左下
	mVertices[1].uv = {texLeft, texTop};     // 左上
	mVertices[2].uv = {texRight, texBottom}; // 右下
	mVertices[3].uv = {texLeft, texTop};     // 左上
	mVertices[4].uv = {texRight, texTop};    // 右上
	mVertices[5].uv = {texRight, texBottom}; // 右下
}

/// @brief スプライトの描画
void Sprite::Draw() const {
	{
		mVertexBuffer->Update(
			mVertices.data(),
			sizeof(Vertex) * mVertices.size()
		);
		mIndexBuffer->Update(mIndices, sizeof(uint16_t) * kSpriteVertexCount);

		// uvTransformから行列を作成
		Mat4 uvTransformMat = Mat4::Scale(mUvTransform.scale);
		uvTransformMat = uvTransformMat * Mat4::RotateZ(mUvTransform.rotate.z);
		uvTransformMat = uvTransformMat * Mat4::Translate(
			                 mUvTransform.translate
		                 );
		// 設定
		mMaterialData->uvTransform = uvTransformMat;

		// 各種行列を作成
		Vec2 viewportSize = Vec2::zero;
		if (auto* engine = Unnamed::EngineServices::Get()) {
			viewportSize = engine->GetViewportSizeInstance();
		}
		if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f) {
			viewportSize = {
				static_cast<float>(OldWindowManager::GetMainWindow()->GetClientWidth()),
				static_cast<float>(OldWindowManager::GetMainWindow()->GetClientHeight())
			};
		}

		const Mat4 worldMat = Mat4::Affine(
			mTransform.scale, mTransform.rotate,
			mTransform.translate
		);
		const Mat4 viewMat = Mat4::identity;
		const Mat4 projMat = Mat4::MakeOrthographicMat(
			0.0f,
			0.0f,
			viewportSize.x,
			viewportSize.y,
			0.0f,
			100.0f
		);

		const TransformationMatrix worldViewProjectionMatrixSprite = {
			worldMat * viewMat * projMat,
			worldMat,
		};

		*mTransformationMatrixData = worldViewProjectionMatrixSprite;
	}

	// 頂点バッファの設定
	const D3D12_VERTEX_BUFFER_VIEW vbView = mVertexBuffer->View();
	mSpriteCommon->GetD3D12()->GetCommandList()->IASetVertexBuffers(
		0, 1, &vbView
	);

	// 定数バッファの設定
	mSpriteCommon->GetD3D12()->GetCommandList()->
	               SetGraphicsRootConstantBufferView(
		               0, mMaterialResource->GetAddress()
	               );
	mSpriteCommon->GetD3D12()->GetCommandList()->
	               SetGraphicsRootConstantBufferView(
		               1, mTransformation->GetAddress()
	               );

	// テクスチャのSRVを設定
	if (auto* engine = Unnamed::EngineServices::Get()) {
		if (auto* texManager = engine->GetTexManagerInstance()) {
		mSpriteCommon->GetD3D12()->GetCommandList()->
		               SetGraphicsRootDescriptorTable(
			               2, texManager->GetSrvHandleGPU(
				               mTextureFilePath
			               )
		               );
		}
	}

	// インデックスバッファの設定
	const D3D12_INDEX_BUFFER_VIEW indexBufferView = mIndexBuffer->View();
	mSpriteCommon->GetD3D12()->GetCommandList()->IASetIndexBuffer(
		&indexBufferView
	);

	// 描画
	mSpriteCommon->GetD3D12()->GetCommandList()->DrawIndexedInstanced(
		6, 1, 0, 0, 0
	);
}

/// @brief スプライトのテクスチャを変更
/// @param textureFilePath テクスチャファイルパス
void Sprite::ChangeTexture(const std::string& textureFilePath) {
	// テクスチャのパスを変更
	this->mTextureFilePath = textureFilePath;
}

/// @brief 座標の取得
Vec3 Sprite::GetPos() const { return mTransform.translate; }

/// @brief 回転の取得
Vec3 Sprite::GetRot() const { return mTransform.rotate; }

/// @brief サイズの取得
Vec3 Sprite::GetSize() const { return mTransform.scale; }

/// @brief アンカーポイントの取得
Vec2 Sprite::GetAnchorPoint() const { return mAnchorPoint; }

/// @brief 色の取得
Vec4 Sprite::GetColor() const { return mMaterialData->color; }

/// @brief テクスチャの左上座標の取得
Vec2 Sprite::GetTextureLeftTop() const { return mTextureLeftTop; }

/// @brief テクスチャサイズの取得
Vec2 Sprite::GetTextureSize() const { return mTextureSize; }

/// @brief X反転しているか取得
bool Sprite::GetIsFlipX() const { return mIsFlipX; }

/// @brief Y反転しているか取得
bool Sprite::GetIsFlipY() const { return mIsFlipY; }

/// @brief UV座標の取得
Vec2 Sprite::GetUvPos() {
	return {mUvTransform.translate.x, mUvTransform.translate.y};
}

/// @brief UVサイズの取得
Vec2 Sprite::GetUvSize() {
	return {mUvTransform.scale.x, mUvTransform.scale.y};
}

/// @brief UV回転の取得
float Sprite::GetUvRot() const { return mUvTransform.rotate.z; }

/// @brief 座標の設定
/// @param newPos 新しい座標
void Sprite::SetPos(const Vec3& newPos) { mTransform.translate = newPos; }

/// @brief 回転の設定
/// @param newRot 新しい回転
void Sprite::SetRot(const Vec3& newRot) { mTransform.rotate = newRot; }

/// @brief サイズの設定
/// @param newSize 新しいサイズ
void Sprite::SetSize(const Vec3& newSize) { mTransform.scale = newSize; }

/// @brief アンカーポイントの設定
/// @param anchorPoint 新しいアンカーポイント
void Sprite::SetAnchorPoint(const Vec2& anchorPoint) {
	this->mAnchorPoint = anchorPoint;
}

/// @brief 色の設定
/// @param color 新しい色
void Sprite::SetColor(const Vec4 color) const { mMaterialData->color = color; }

/// @brief X反転の設定
/// @param isFlipX X反転フラグ
void Sprite::SetIsFlipX(const bool isFlipX) { mIsFlipX = isFlipX; }

/// @brief Y反転の設定
/// @param isFlipY Y反転フラグ
void Sprite::SetIsFlipY(const bool isFlipY) { mIsFlipY = isFlipY; }

/// @brief テクスチャの左上座標の設定
/// @param newTextureLeftTop 新しいテクスチャ左上座標
void Sprite::SetTextureLeftTop(const Vec2& newTextureLeftTop) {
	this->mTextureLeftTop = newTextureLeftTop;
}

/// @brief テクスチャサイズの設定
/// @param newTextureSize 新しいテクスチャサイズ
void Sprite::SetTextureSize(const Vec2& newTextureSize) {
	this->mTextureSize = newTextureSize;
}

/// @brief UV座標の設定
/// @param newPos 新しいUV座標
void Sprite::SetUvPos(const Vec2& newPos) {
	for (uint32_t i = 0; i < 2; ++i) { mUvTransform.translate[i] = newPos[i]; }
}

/// @brief UVサイズの設定
/// @param newSize 新しいUVサイズ
void Sprite::SetUvSize(const Vec2& newSize) {
	for (uint32_t i = 0; i < 2; ++i) { mUvTransform.scale[i] = newSize[i]; }
}

/// @brief UV回転の設定
/// @param newRot 新しいUV回転
void Sprite::SetUvRot(const float newRot) { mUvTransform.rotate.z = newRot; }

/// @brief テクスチャサイズに合わせてスプライトのサイズを調整
void Sprite::AdjustTextureSize() {
	if (auto* engine = Unnamed::EngineServices::Get()) {
		auto* texManager = engine->GetTexManagerInstance();
		if (!texManager) { return; }
		mTextureResourceSize = {
			static_cast<float>(texManager->GetMetaData(
				mTextureFilePath
			).width),
			static_cast<float>(texManager->GetMetaData(
				mTextureFilePath
			).height)
		};
	}

	mTextureLeftTop     = Vec2::zero;
	mTextureSize        = mTextureResourceSize;
	mTransform.scale.x  = mTextureResourceSize.x;
	mTransform.scale.y  = mTextureResourceSize.y;
}
