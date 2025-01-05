#include "Line.h"

#include <mutex>

#include "../Camera/Camera.h"
#include "../Lib/Console/Console.h"

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

Line::Line(LineCommon* lineCommon) {
	lineCommon_ = lineCommon;
	constexpr size_t vertexBufferSize = kMaxLineCount * 2 * sizeof(LineVertex); // 頂点数 = ライン数 * 2
	constexpr size_t indexBufferSize = kMaxLineCount * 2 * sizeof(uint32_t); // インデックス数 = ライン数 * 2
	vertexBuffer_ = std::make_unique<VertexBuffer<LineVertex>>(
		lineCommon_->GetD3D12()->GetDevice(),
		vertexBufferSize,
		nullptr
	);
	indexBuffer_ = std::make_unique<IndexBuffer>(
		lineCommon_->GetD3D12()->GetDevice(),
		indexBufferSize,
		nullptr
	);

	transformationMatrixConstantBuffer_ = std::make_unique<ConstantBuffer>(
		lineCommon_->GetD3D12()->GetDevice(), sizeof(TransformationMatrix)
	);
	transformationMatrixData_ = transformationMatrixConstantBuffer_->GetPtr<TransformationMatrix>();
	transformationMatrixData_->wvp = Mat4::identity;
	transformationMatrixData_->world = Mat4::identity;
}

static std::mutex lineMutex;

void Line::AddLine(const Vec3& start, const Vec3& end, const Vec4& color) {
	std::lock_guard lock(lineMutex);

	// 頂点を追加
	const uint32_t startIndex = static_cast<uint32_t>(lineVertices_.size());

	// インデックスを正確に追加 (最後の2つのインデックスのみを追加)
	lineVertices_.push_back({.pos = start, .color = color});
	lineVertices_.push_back({.pos = end, .color = color});

	lineIndices_.push_back(startIndex); // 開始頂点
	lineIndices_.push_back(startIndex + 1); // 終了頂点

	isDirty_ = true; // バッファを更新する
}

void Line::Update() {
	const Mat4& viewProjMat = lineCommon_->GetDefaultCamera()->GetViewProjMat();

	transformationMatrixData_->wvp = viewProjMat;

	UpdateBuffer();
}

void Line::UpdateBuffer() {
	// 更新の必要がない、またはデータが空の場合は終了
	if (!isDirty_ || lineVertices_.empty())
	{
		return;
	}

	// 必要なサイズを正確に計算
	const size_t requiredVertexBufferSize = sizeof(LineVertex) * lineVertices_.size();
	const size_t requiredIndexBufferSize = sizeof(uint32_t) * lineIndices_.size();

	// バッファが不足している場合は再作成
	if (vertexBuffer_->GetSize() < requiredVertexBufferSize)
	{
		Console::Print("Line: VertexBufferを再作成します。\n", kConsoleColorWarning);
		vertexBuffer_ = std::make_unique<VertexBuffer<LineVertex>>(
			lineCommon_->GetD3D12()->GetDevice(),
			requiredVertexBufferSize,
			nullptr
		);
	}

	if (indexBuffer_->GetSize() < requiredIndexBufferSize)
	{
		Console::Print("Line: IndexBufferを再作成します。\n", kConsoleColorWarning);
		indexBuffer_ = std::make_unique<IndexBuffer>(
			lineCommon_->GetD3D12()->GetDevice(),
			requiredIndexBufferSize,
			nullptr
		);
	}

	// バッファを更新
	vertexBuffer_->Update(lineVertices_.data(), lineVertices_.size() * sizeof(LineVertex));
	indexBuffer_->Update(lineIndices_.data(), lineIndices_.size() * sizeof(uint32_t));

	isDirty_ = false; // バッファは最新状態
}

void Line::Draw() {
	if (lineVertices_.empty() || lineIndices_.empty())
	{
		return;
	}

	lineCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(
		0, transformationMatrixConstantBuffer_->GetAddress()
	);

	const D3D12_VERTEX_BUFFER_VIEW vbView = vertexBuffer_->View();
	const D3D12_INDEX_BUFFER_VIEW ibView = indexBuffer_->View();

	lineCommon_->GetD3D12()->GetCommandList()->IASetVertexBuffers(0, 1, &vbView);
	lineCommon_->GetD3D12()->GetCommandList()->IASetIndexBuffer(&ibView);

	lineCommon_->GetD3D12()->GetCommandList()->DrawIndexedInstanced(
		static_cast<UINT>(lineIndices_.size()), // インデックス数
		1, // インスタンス数
		0, // 開始インデックス位置
		0, // ベース頂点位置
		0 // 開始インスタンス位置
	);

	lineVertices_.clear();
	lineIndices_.clear();
}
