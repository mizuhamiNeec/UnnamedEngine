#pragma once
#include <memory>

#include <runtime/core/math/Math.h>

#include "engine/renderer/ConstantBuffer.h"

#include "engine/renderer/IndexBuffer.h"
#include "engine/renderer/Structs.h"
#include "engine/renderer/VertexBuffer.h"

// スプライトの頂点数
constexpr uint32_t kSpriteVertexCount = 6;

class SpriteCommon;

/// @brief スプライトクラス
class Sprite final {
public:
	~Sprite();

	void Init(SpriteCommon* spriteCommon, const std::string& textureFilePath);
	void Update();
	void Draw() const;

	void ChangeTexture(const std::string& textureFilePath);

	// Getter
	Vec3  GetPos() const;
	Vec3  GetRot() const;
	Vec3  GetSize() const;
	Vec2  GetAnchorPoint() const;
	Vec4  GetColor() const;
	Vec2  GetTextureLeftTop() const;
	Vec2  GetTextureSize() const;
	bool  GetIsFlipX() const;
	bool  GetIsFlipY() const;
	Vec2  GetUvPos();
	Vec2  GetUvSize();
	float GetUvRot() const;

	// Setter
	void SetPos(const Vec3& newPos);
	void SetRot(const Vec3& newRot);
	void SetSize(const Vec3& newSize);
	void SetAnchorPoint(const Vec2& anchorPoint);
	void SetColor(Vec4 color) const;
	void SetIsFlipX(bool isFlipX);
	void SetIsFlipY(bool isFlipY);
	void SetTextureLeftTop(const Vec2& newTextureLeftTop);
	void SetTextureSize(const Vec2& newTextureSize);
	void SetUvPos(const Vec2& newPos);
	void SetUvSize(const Vec2& newSize);
	void SetUvRot(float newRot);

private:
	struct Material {
		Vec4    color;
		int32_t enableLighting;
		Vec3    padding;
		Mat4    uvTransform;
		float   shininess;
		Vec3    specularColor;
	};

	// テクスチャサイズをイメージに合わせる
	void AdjustTextureSize();

	SpriteCommon* mSpriteCommon = nullptr;

	Vec2 mAnchorPoint = {0.0f, 0.0f};

	Transform mTransform;
	Transform mUvTransform;

	// 左右フリップ
	bool mIsFlipX = false;
	// 上下フリップ
	bool mIsFlipY = false;

	// テクスチャ左上座標
	Vec2 mTextureLeftTop = {0.0f, 0.0f};
	Vec2 mTextureSize    = {100.0f, 100.0f};
	Vec2 mTextureResourceSize = {1.0f, 1.0f};

	// テクスチャ番号
	std::string mTextureFilePath;

	std::vector<Vertex> mVertices;

	uint32_t mIndices[kSpriteVertexCount] = {
		0, 1, 2, // 1つ目の三角形
		1, 4, 2  // 2つ目の三角形
	};

	// バッファリソース
	std::unique_ptr<VertexBuffer<Vertex>> mVertexBuffer     = nullptr;
	std::unique_ptr<ConstantBuffer>       mMaterialResource = nullptr;
	std::unique_ptr<ConstantBuffer>       mTransformation   = nullptr;
	std::unique_ptr<IndexBuffer>          mIndexBuffer      = nullptr;
	// バッファリソース内のデータを指すポインタ
	Vertex*               mVertexData               = nullptr;
	uint32_t*             mIndexData                = nullptr;
	Material*             mMaterialData             = nullptr;
	TransformationMatrix* mTransformationMatrixData = nullptr;
};
