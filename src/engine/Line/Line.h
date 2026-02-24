#pragma once
#include <runtime/core/math/Math.h>

#include <engine/Line/LineCommon.h>

#include <engine/renderer/ConstantBuffer.h>
#include <engine/renderer/D3D12.h>
#include <engine/renderer/IndexBuffer.h>
#include <engine/renderer/VertexBuffer.h>

#include <core/unnamed/containers/RingBuffer.h>

struct TransformationMatrix;
constexpr size_t kMaxLineCount = 65536;

/// @brief ライン描画用の頂点構造体
struct LineVertex {
	Vec3 pos;
	Vec4 color;

	static const D3D12_INPUT_LAYOUT_DESC inputLayout;

private:
	static constexpr int                  inputElementCount = 2;
	static const D3D12_INPUT_ELEMENT_DESC inputElements[inputElementCount];
};

struct LineSegment {
	LineVertex start;
	LineVertex end;
};

/// @brief ライン描画クラス
class Line {
	// 座標変換行列データ
	struct TransformationMatrix {
		Mat4 wvp;
		Mat4 world;
		Mat4 worldInverseTranspose;
	};

public:
	explicit Line(LineCommon* lineCommon);

	void AddLine(Vec3 start, Vec3 end, const Vec4& color);
	void Draw();

	void ReserveLines(size_t lineCount);

	void Clear();

private:
	void UpdateBuffer();

	//-------------------------------------------------------------------------
	LineCommon* mLineCommon = nullptr;

	Unnamed::RingBuffer<LineSegment, kMaxLineCount> mLineSegments;

	std::vector<LineVertex> mLineVertices;
	std::vector<uint32_t>   mLineIndices;

	std::unique_ptr<VertexBuffer<LineVertex>> mVertexBuffer;
	std::unique_ptr<IndexBuffer>              mIndexBuffer;

	TransformationMatrix* mTransformationMatrixData = nullptr; // 座標変換行列のポインタ
	std::unique_ptr<ConstantBuffer> mTransformationMatrixConstantBuffer;

	bool mIsDirty = true; // バッファ更新の必要があるか?
};
