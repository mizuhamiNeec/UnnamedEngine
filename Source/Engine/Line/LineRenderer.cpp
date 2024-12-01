#include "LineRenderer.h"

const D3D12_INPUT_ELEMENT_DESC LineVertex::inputElements[] = {
	{
		"POSITION",
		0,
		DXGI_FORMAT_R32G32B32_FLOAT,
		0,
		0,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	},
	{
		"COLOR",
		0,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		0,
		12,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	}
};

const D3D12_INPUT_LAYOUT_DESC LineVertex::inputLayout = {
	inputElements,
	inputElementCount
};

LineRenderer::LineRenderer(D3D12* d3d12) {
	d3d12_ = d3d12;
	size_t vertexBufferSize = kMaxLineCount * 2 * sizeof(LineVertex);
	size_t indexBufferSize = kMaxLineCount * 2 * sizeof(uint32_t);
	vertexBuffer_ = std::make_unique<VertexBuffer>(
		d3d12_->GetDevice(),
		vertexBufferSize,
		sizeof(LineVertex),
		nullptr
	);
	indexBuffer_ = std::make_unique<IndexBuffer>(
		d3d12_->GetDevice(),
		indexBufferSize,
		nullptr
	);
}

void LineRenderer::AddLine(const Vec3& start, const Vec3& end, const Vec4& color) {
	// 頂点を追加
	const uint32_t startIndex = static_cast<uint32_t>(lineVertices_.size());
	const uint32_t endIndex = startIndex + 1;

	// 配列にぶち込む
	lineVertices_.push_back({.pos = start, .color = color});
	lineVertices_.push_back({.pos = end, .color = color});

	// インデックスを追加
	lineIndices_.push_back(startIndex);
	lineIndices_.push_back(endIndex);

	isDirty_ = true; // バッファを更新する
}

void LineRenderer::UpdateBuffer() {
	// 更新する必要もない、配列も空
	if (!isDirty_ || lineVertices_.empty()) {
		return; // さいなら
	}

	const size_t vertexBufferSize = sizeof(LineVertex) * lineVertices_.size();
	const size_t indexBufferSize = sizeof(uint32_t) * lineIndices_.size();

	vertexBuffer_->Update(lineVertices_.data(), vertexBufferSize);
	indexBuffer_->Update(lineIndices_.data(), indexBufferSize);

	isDirty_ = false; // バッファは最新
}

void LineRenderer::CreateRootSignature() {
	rootSignatureManager_ = new RootSignatureManager(d3d12_->GetDevice());

	D3D12_DESCRIPTOR_RANGE descriptorRange[1];
	descriptorRange[0] = {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		.NumDescriptors = 1,
		.BaseShaderRegister = 0,
		.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
	};

	std::vector<D3D12_ROOT_PARAMETER> rootParameters(2)
	=
	{
	}
}

void LineRenderer::Draw() {
	if (lineVertices_.empty() || lineIndices_.empty()) {
		return;
	}

	UpdateBuffer();

	const D3D12_VERTEX_BUFFER_VIEW vbView = vertexBuffer_->View();
	const D3D12_INDEX_BUFFER_VIEW ibView = indexBuffer_->View();

	d3d12_->GetCommandList()->SetPipelineState();
	d3d12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	d3d12_->GetCommandList()->IASetVertexBuffers(0, 1, &vbView);
	d3d12_->GetCommandList()->IASetIndexBuffer(&ibView);

	d3d12_->GetCommandList()->DrawIndexedInstanced(static_cast<UINT>(lineIndices_.size()), 1, 0, 0, 0);

	lineVertices_.clear();
	lineIndices_.clear();
}
