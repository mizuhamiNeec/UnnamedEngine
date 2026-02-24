#include "Line.h"

#include <d3d12.h>

#include <algorithm>
#include <mutex>

#include "engine/Camera/CameraManager.h"

#include "engine/Components/Camera/CameraComponent.h"

#include "engine/OldConsole/Console.h"

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

/// @brief Lineクラスのコンストラクタ
/// @param lineCommon LineCommonクラスへのポインタ
Line::Line(LineCommon* lineCommon) {
	mLineCommon                       = lineCommon;
	constexpr size_t vertexBufferSize = kMaxLineCount * 2 * sizeof(LineVertex);
	// 頂点数 = ライン数 * 2
	constexpr size_t indexBufferSize = kMaxLineCount * 2 * sizeof(uint32_t);
	// インデックス数 = ライン数 * 2
	mVertexBuffer = std::make_unique<VertexBuffer<LineVertex>>(
		mLineCommon->GetRenderer()->GetDevice(),
		vertexBufferSize,
		nullptr
	);
	mIndexBuffer = std::make_unique<IndexBuffer>(
		mLineCommon->GetRenderer()->GetDevice(),
		indexBufferSize,
		nullptr
	);

	// バッファ上限に合わせて先に確保して再確保を抑える
	mLineVertices.reserve(kMaxLineCount * 2);
	mLineIndices.reserve(kMaxLineCount * 2);

	mTransformationMatrixConstantBuffer = std::make_unique<ConstantBuffer>(
		mLineCommon->GetRenderer()->GetDevice(), sizeof(TransformationMatrix),
		"LineTransformation"
	);
	mTransformationMatrixData = mTransformationMatrixConstantBuffer->GetPtr<
		TransformationMatrix>();
	mTransformationMatrixData->wvp   = Mat4::identity;
	mTransformationMatrixData->world = Mat4::identity;
}

static std::mutex lineMutex;

/// @brief ラインを追加する
/// @param start ラインの開始点
/// @param end ラインの終了点
/// @param color ラインの色
void Line::AddLine(const Vec3 start, const Vec3 end, const Vec4& color) {
#ifndef _DEBUG
	return;
#endif

	std::lock_guard lock(lineMutex);

	mLineSegments.Push(LineSegment{
		.start = LineVertex{start, color},
		.end   = LineVertex{end, color}
	});

	mIsDirty = true;
}

void Line::ReserveLines(const size_t lineCount) {
	std::lock_guard lock(lineMutex);
	// 1ライン = 頂点2つ + インデックス2つ
	const size_t clamped = std::min(lineCount, kMaxLineCount);
	mLineVertices.reserve(clamped * 2);
	mLineIndices.reserve(clamped * 2);
}

void Line::Clear() {
	std::lock_guard lock(lineMutex);
	mLineSegments.Clear();
	mLineVertices.clear();
	mLineIndices.clear();
	mIsDirty = true;
}

/// @brief 頂点バッファとインデックスバッファを更新する
void Line::UpdateBuffer() {
	std::lock_guard lock(lineMutex);

	mLineVertices.clear();
	mLineIndices.clear();

	mLineVertices.reserve(kMaxLineCount * 2);
	mLineIndices.reserve(kMaxLineCount * 2);

	for (const auto& segment : mLineSegments) {
		const uint32_t startIndex = static_cast<uint32_t>(mLineVertices.size());
		mLineVertices.emplace_back(segment.start);
		mLineVertices.emplace_back(segment.end);
		mLineIndices.emplace_back(startIndex);
		mLineIndices.emplace_back(startIndex + 1);
	}

	if (mLineVertices.empty()) {
		mIsDirty = false;
		return;
	}

	// 必要なサイズを正確に計算
	const size_t requiredVertexBufferSize =
		sizeof(LineVertex) * mLineVertices.size();
	const size_t requiredIndexBufferSize =
		sizeof(uint32_t) * mLineIndices.size();

	auto grow = [](size_t required) {
		const size_t grown = static_cast<size_t>(
			                     static_cast<double>(required) * 1.5) + 256;
		return std::max(required, grown);
	};

	if (mVertexBuffer->GetSize() < requiredVertexBufferSize) {
		Console::Print("Line: VertexBufferを再作成します。\n", kConTextColorWarning);
		mVertexBuffer = std::make_unique<VertexBuffer<LineVertex>>(
			mLineCommon->GetRenderer()->GetDevice(),
			grow(requiredVertexBufferSize),
			nullptr
		);
	}

	if (mIndexBuffer->GetSize() < requiredIndexBufferSize) {
		Console::Print("Line: IndexBufferを再作成します。\n", kConTextColorWarning);
		mIndexBuffer = std::make_unique<IndexBuffer>(
			mLineCommon->GetRenderer()->GetDevice(),
			grow(requiredIndexBufferSize),
			nullptr
		);
	}

	mVertexBuffer->Update(mLineVertices.data(), requiredVertexBufferSize);
	mIndexBuffer->Update(mLineIndices.data(), requiredIndexBufferSize);

	mIsDirty = false; // バッファは最新状態
}

/// @brief ラインを描画する
void Line::Draw() {
	if (mIsDirty) { UpdateBuffer(); }

	if (mLineVertices.empty() || mLineIndices.empty()) { return; }

	// ビュープロジェクション行列の設定
	const Mat4& viewProjMat = CameraManager::GetActiveCamera()->
		GetViewProjMat();
	mTransformationMatrixData->wvp = viewProjMat;

	mLineCommon->GetRenderer()->GetCommandList()->
	             SetGraphicsRootConstantBufferView(
		             0, mTransformationMatrixConstantBuffer->GetAddress()
	             );

	const D3D12_VERTEX_BUFFER_VIEW vbView = mVertexBuffer->View();
	const D3D12_INDEX_BUFFER_VIEW  ibView = mIndexBuffer->View();

	mLineCommon->GetRenderer()->GetCommandList()->IASetVertexBuffers(
		0, 1, &vbView
	);
	mLineCommon->GetRenderer()->GetCommandList()->IASetIndexBuffer(&ibView);

	// パイプラインステートとルートシグネチャの設定
	mLineCommon->Render();

	mLineCommon->GetRenderer()->GetCommandList()->DrawIndexedInstanced(
		static_cast<UINT>(mLineIndices.size()),
		1,
		0,
		0,
		0
	);

	Clear();
}
