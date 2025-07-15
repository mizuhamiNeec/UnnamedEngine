#include "Line.h"

#include <mutex>
#include <Camera/CameraManager.h>

#include <Components/Camera/CameraComponent.h>

#include <SubSystem/Console/Console.h>

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
	mLineCommon                       = lineCommon;
	constexpr size_t vertexBufferSize = kMaxLineCount * 2 * sizeof(LineVertex);
	// 頂点数 = ライン数 * 2
	constexpr size_t indexBufferSize = kMaxLineCount * 2 * sizeof(uint32_t);
	// インデックス数 = ライン数 * 2
	mVertexBuffer = std::make_unique<VertexBuffer<LineVertex>>(
		mLineCommon->GetRenderer()->GetDevice(),
		vertexBufferSize,
		nullptr);
	mIndexBuffer = std::make_unique<IndexBuffer>(
		mLineCommon->GetRenderer()->GetDevice(),
		indexBufferSize,
		nullptr);

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

void Line::AddLine(const Vec3& start, const Vec3& end, const Vec4& color) {
	std::lock_guard lock(lineMutex);

	// 頂点を追加
	const uint32_t startIndex = static_cast<uint32_t>(mLineVertices.size());

	// インデックスを正確に追加 (最後の2つのインデックスのみを追加)
	mLineVertices.emplace_back(LineVertex(start, color));
	mLineVertices.emplace_back(LineVertex(end, color));

	mLineIndices.emplace_back(startIndex);     // 開始頂点
	mLineIndices.emplace_back(startIndex + 1); // 終了頂点

	mIsDirty = true; // バッファを更新する
}

void Line::UpdateBuffer() {
	// 更新の必要がない、またはデータが空の場合は終了
	if (!mIsDirty || mLineVertices.empty()) {
		return;
	}

	// 必要なサイズを正確に計算
	const size_t requiredVertexBufferSize = sizeof(LineVertex) * mLineVertices.
		size();
	const size_t requiredIndexBufferSize = sizeof(uint32_t) * mLineIndices.
		size();

	// バッファが不足している場合は再作成
	if (mVertexBuffer->GetSize() < requiredVertexBufferSize) {
		Console::Print("Line: VertexBufferを再作成します。\n", kConTextColorWarning);
		mVertexBuffer = std::make_unique<VertexBuffer<LineVertex>>(
			mLineCommon->GetRenderer()->GetDevice(),
			requiredVertexBufferSize,
			nullptr);
	}

	if (mIndexBuffer->GetSize() < requiredIndexBufferSize) {
		Console::Print("Line: IndexBufferを再作成します。\n", kConTextColorWarning);
		mIndexBuffer = std::make_unique<IndexBuffer>(
			mLineCommon->GetRenderer()->GetDevice(),
			requiredIndexBufferSize,
			nullptr);
	}

	// バッファを更新
	mVertexBuffer->Update(mLineVertices.data(),
	                      mLineVertices.size() * sizeof(LineVertex));
	mIndexBuffer->Update(mLineIndices.data(),
	                     mLineIndices.size() * sizeof(uint32_t));

	mIsDirty = false; // バッファは最新状態
}

void Line::Draw() {
	if (mIsDirty) {
		UpdateBuffer();
	}

	if (mLineVertices.empty() || mLineIndices.empty()) {
		return;
	}

	// ビュープロジェクション行列の設定
	const Mat4& viewProjMat = CameraManager::GetActiveCamera()->
		GetViewProjMat();
	mTransformationMatrixData->wvp = viewProjMat;

	mLineCommon->GetRenderer()->GetCommandList()->
	             SetGraphicsRootConstantBufferView(
		             0, mTransformationMatrixConstantBuffer->GetAddress());

	const D3D12_VERTEX_BUFFER_VIEW vbView = mVertexBuffer->View();
	const D3D12_INDEX_BUFFER_VIEW  ibView = mIndexBuffer->View();

	mLineCommon->GetRenderer()->GetCommandList()->IASetVertexBuffers(
		0, 1, &vbView);
	mLineCommon->GetRenderer()->GetCommandList()->IASetIndexBuffer(&ibView);

	// パイプラインステートとルートシグネチャの設定
	mLineCommon->Render();

	mLineCommon->GetRenderer()->GetCommandList()->DrawIndexedInstanced(
		static_cast<UINT>(mLineIndices.size()), // インデックス数
		1,                                      // インスタンス数
		0,                                      // 開始インデックス位置
		0,                                      // ベース頂点位置
		0                                       // 開始インスタンス位置
	);

	mLineVertices.clear();
	mLineIndices.clear();
}
