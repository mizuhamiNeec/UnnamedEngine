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
	void Update();
	void Draw();

	void SetTransform(const Transform& newTransform);
	void SetPos(const Vec3& newPos);
	void SetRot(const Vec3& newRot);
	void SetScale(const Vec3& newScale);
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

