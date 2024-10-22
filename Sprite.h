#pragma once
#include <memory>

#include "Source/Engine/Lib/Math/Vector/Vec2.h"
#include "Source/Engine/Lib/Structs/Structs.h"
#include "Source/Engine/Lib/Transform/Transform.h"
#include "Source/Engine/Renderer/ConstantBuffer.h"
#include "Source/Engine/Renderer/VertexBuffer.h"
#include "Source/Engine/Renderer/IndexBuffer.h"

// スプライトの頂点数
constexpr uint32_t kSpriteVertexCount = 6;

class SpriteCommon;

class Sprite {
public:
	~Sprite();

	void Init(SpriteCommon* spriteCommon, const std::string& textureFilePath);
	void Update();
	void Draw() const;

	void ChangeTexture(const std::string& textureFilePath);

	// Getter
	Vec3 GetPos() const;
	Vec3 GetRot() const;
	Vec3 GetSize() const;
	Vec2 GetAnchorPoint() const;
	Vec4 GetColor() const;
	bool GetIsFlipX() const;
	bool GetIsFlipY() const;
	Vec2 GetUvPos();
	Vec2 GetUvSize();
	float GetUvRot() const;

	// Setter
	void SetPos(const Vec3& newPos);
	void SetRot(const Vec3& newRot);
	void SetSize(const Vec3& newSize);
	void SetAnchorPoint(const Vec2& anchorPoint);
	void SetColor(Vec4 color) const;
	void SetIsFlipX(bool isFlipX);
	void SetIsFlipY(bool isFlipY);
	void SetUvPos(const Vec2& newPos);
	void SetUvSize(const Vec2& newSize);
	void SetUvRot(const float& newRot);

private:
	SpriteCommon* spriteCommon_ = nullptr;

	Vec2 anchorPoint_ = { 0.0f,0.0f };

	Transform transform_;
	Transform uvTransform_;

	// 左右フリップ
	bool isFlipX_ = false;
	// 上下フリップ
	bool isFlipY_ = false;

	// テクスチャ番号
	uint32_t textureIndex_ = 0;

	Vertex vertices[kSpriteVertexCount] = {};

	uint32_t indices[kSpriteVertexCount] = {
		0, 1, 2,  // 1つ目の三角形
		1, 4, 2 // 2つ目の三角形
	};

	// バッファリソース
	std::unique_ptr<VertexBuffer> vertexBuffer_ = nullptr;
	std::unique_ptr<ConstantBuffer> materialResource_ = nullptr;
	std::unique_ptr<ConstantBuffer> transformation_ = nullptr;
	std::unique_ptr<IndexBuffer> indexBuffer_ = nullptr;
	// バッファリソース内のデータを指すポインタ
	Vertex* vertexData_ = nullptr;
	uint32_t* indexData = nullptr;
	Material* materialData_ = nullptr;
	TransformationMatrix* transformationMatrixData_ = nullptr;
};

