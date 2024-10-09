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

	void Init(SpriteCommon* spriteCommon);
	void Update() const;
	void Draw() const;

	// Getter
	Vec3 GetPos();
	Vec3 GetRot();
	Vec3 GetSize();
	Vec4 GetColor() const;
	Vec2 GetUVPos();
	Vec2 GetUVSize();
	float GetUVRot() const;

	// Setter
	void SetPos(const Vec3& newPos);
	void SetRot(const Vec3& newRot);
	void SetSize(const Vec3& newSize);
	void SetColor(Vec4 color) const;
	void SetUVPos(const Vec2& newPos);
	void SetUVSize(const Vec2& newSize);
	void SetUVRot(const float& newRot);
private:
	SpriteCommon* spriteCommon_ = nullptr;

	Transform transform_;
	Transform uvTransform_;

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

