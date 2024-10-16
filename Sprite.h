#pragma once
#include <memory>

#include "Source/Engine/Lib/Math/Vector/Vec2.h"
#include "Source/Engine/Lib/Structs/Structs.h"
#include "Source/Engine/Lib/Transform/Transform.h"
#include "Source/Engine/Renderer/ConstantBuffer.h"
#include "Source/Engine/Renderer/VertexBuffer.h"
#include "Source/Engine/Renderer/IndexBuffer.h"

class SpriteCommon;

class Sprite {
public:
	~Sprite();

	void Init(SpriteCommon* spriteCommon, const std::string& textureFilePath);
	void Update() const;
	void Draw() const;

	void ChangeTexture(const std::string& textureFilePath);

	// Getter
	Vec3 GetPos() const;
	Vec3 GetRot() const;
	Vec3 GetSize() const;
	Vec4 GetColor() const;
	Vec2 GetUvPos();
	Vec2 GetUvSize();
	float GetUvRot() const;

	// Setter
	void SetPos(const Vec3& newPos);
	void SetRot(const Vec3& newRot);
	void SetSize(const Vec3& newSize);
	void SetColor(Vec4 color) const;
	void SetUvPos(const Vec2& newPos);
	void SetUvSize(const Vec2& newSize);
	void SetUvRot(const float& newRot);
private:
	SpriteCommon* spriteCommon_ = nullptr;

	Transform transform_;
	Transform uvTransform_;

	// テクスチャ番号
	uint32_t textureIndex_ = 0;

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

