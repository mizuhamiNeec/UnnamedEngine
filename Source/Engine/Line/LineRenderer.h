#pragma once

#include "../Renderer/D3D12.h"
#include "../Renderer/VertexBuffer.h"
#include "../Renderer/IndexBuffer.h"
#include "../Renderer/RootSignatureManager.h"
#include "../Renderer/PipelineState.h"

#include "../Lib/Math/Vector/Vec3.h"
#include "../Lib/Math/Vector/Vec4.h"

constexpr size_t kMaxLineCount = 65535;

struct LineVertex {
	Vec3 pos;
	Vec4 color;

	static const D3D12_INPUT_LAYOUT_DESC inputLayout;

private:
	static constexpr int inputElementCount = 2;
	static const D3D12_INPUT_ELEMENT_DESC inputElements[inputElementCount];
};

class LineRenderer {
public:
	explicit LineRenderer(D3D12* d3d12);

	void AddLine(const Vec3& start, const Vec3& end, const Vec4& color);
	void Draw();

private:
	void UpdateBuffer();
	void CreateRootSignature();

	//-------------------------------------------------------------------------
	D3D12* d3d12_ = nullptr; // レンダラ

	std::vector<LineVertex> lineVertices_;
	std::vector<uint32_t> lineIndices_;

	std::unique_ptr<VertexBuffer> vertexBuffer_;
	std::unique_ptr<IndexBuffer> indexBuffer_;

	bool isDirty_ = true; // バッファ更新の必要があるか?

	RootSignatureManager* rootSignatureManager_ = nullptr;
	PipelineState pipelineState_;
};
