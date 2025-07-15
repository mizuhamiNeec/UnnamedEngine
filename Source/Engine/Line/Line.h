#pragma once

#include <Lib/Math/Matrix/Mat4.h>
#include <Lib/Math/Vector/Vec3.h>
#include <Lib/Math/Vector/Vec4.h>

#include <Line/LineCommon.h>

#include <Renderer/ConstantBuffer.h>
#include <Renderer/D3D12.h>
#include <Renderer/IndexBuffer.h>
#include <Renderer/VertexBuffer.h>

struct TransformationMatrix;
constexpr size_t kMaxLineCount = 256;

struct LineVertex {
	Vec3 pos;
	Vec4 color;

	static const D3D12_INPUT_LAYOUT_DESC inputLayout;

private:
	static constexpr int inputElementCount = 2;
	static const D3D12_INPUT_ELEMENT_DESC inputElements[inputElementCount];
};

class Line {
	// 座標変換行列データ
	struct TransformationMatrix {
		Mat4 wvp;
		Mat4 world;
		Mat4 worldInverseTranspose;
	};

public:
	explicit Line(LineCommon* lineCommon);

	void AddLine(const Vec3& start, const Vec3& end, const Vec4& color);
	void Draw();

private:
	void UpdateBuffer();

	//-------------------------------------------------------------------------
	LineCommon* mLineCommon = nullptr;

	std::vector<LineVertex> mLineVertices;
	std::vector<uint32_t> mLineIndices;

	std::unique_ptr<VertexBuffer<LineVertex>> mVertexBuffer;
	std::unique_ptr<IndexBuffer> mIndexBuffer;

	TransformationMatrix* mTransformationMatrixData = nullptr; // 座標変換行列のポインタ
	std::unique_ptr<ConstantBuffer> mTransformationMatrixConstantBuffer;

	bool mIsDirty = true; // バッファ更新の必要があるか?
};
