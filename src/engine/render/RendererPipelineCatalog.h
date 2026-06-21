#pragma once

#include <string>

#include "shaders/PipelineRegistry.h"

namespace Unnamed::Render {
	/// @brief レンダラ向けパイプライン定義のプリセット生成を担います。
	class RendererPipelineCatalog {
	public:
		/// @brief フルスクリーン描画向けグラフィクスパイプライン仕様を生成します。
		/// @param debugName デバッグ表示用名
		/// @param shaderProgramId シェーダープログラムアセットID
		/// @param rootSignature ルートシグネチャ
		/// @param rtvFormat レンダーターゲットフォーマット
		/// @return 生成されたパイプライン仕様
		[[nodiscard]] static GraphicsPipelineSpec MakeFullscreenPreset(
			std::string          debugName,
			AssetID              shaderProgramId,
			ID3D12RootSignature* rootSignature,
			DXGI_FORMAT          rtvFormat
		);

		/// @brief ジオメトリ描画向けグラフィクスパイプライン仕様を生成します。
		/// @param debugName デバッグ表示用名
		/// @param shaderProgramId シェーダープログラムアセットID
		/// @param rootSignature ルートシグネチャ
		/// @param rtvFormat カラーバッファフォーマット
		/// @param dsvFormat 深度バッファフォーマット
		/// @param vertexLayout 頂点レイアウト
		/// @return 生成されたパイプライン仕様
		[[nodiscard]] static GraphicsPipelineSpec MakeGeometryPreset(
			std::string                  debugName,
			AssetID                      shaderProgramId,
			ID3D12RootSignature*         rootSignature,
			DXGI_FORMAT                  rtvFormat,
			DXGI_FORMAT                  dsvFormat,
			const Rhi::VertexLayoutDesc& vertexLayout
		);

		/// @brief スプライト描画向けグラフィクスパイプライン仕様を生成します。
		/// @param debugName デバッグ表示用名
		/// @param shaderProgramId シェーダープログラムアセットID
		/// @param rootSignature ルートシグネチャ
		/// @param rtvFormat レンダーターゲットフォーマット
		/// @param vertexLayout 頂点レイアウト
		/// @return 生成されたパイプライン仕様
		[[nodiscard]] static GraphicsPipelineSpec MakeSpritePreset(
			std::string                  debugName,
			AssetID                      shaderProgramId,
			ID3D12RootSignature*         rootSignature,
			DXGI_FORMAT                  rtvFormat,
			const Rhi::VertexLayoutDesc& vertexLayout
		);

		/// @brief ライン描画向けグラフィクスパイプライン仕様を生成します。
		/// @param debugName デバッグ表示用名
		/// @param shaderProgramId シェーダープログラムアセットID
		/// @param rootSignature ルートシグネチャ
		/// @param rtvFormat カラーバッファフォーマット
		/// @param dsvFormat 深度バッファフォーマット
		/// @param vertexLayout 頂点レイアウト
		/// @return 生成されたパイプライン仕様
		[[nodiscard]] static GraphicsPipelineSpec MakeLinePreset(
			std::string                  debugName,
			AssetID                      shaderProgramId,
			ID3D12RootSignature*         rootSignature,
			DXGI_FORMAT                  rtvFormat,
			DXGI_FORMAT                  dsvFormat,
			const Rhi::VertexLayoutDesc& vertexLayout
		);

		/// @brief コンピュート描画向けパイプライン仕様を生成します。
		/// @param debugName デバッグ表示用名
		/// @param shaderProgramId シェーダープログラムアセットID
		/// @param rootSignature ルートシグネチャ
		/// @return 生成されたパイプライン仕様
		[[nodiscard]] static ComputePipelineSpec MakeComputePreset(
			std::string          debugName,
			AssetID              shaderProgramId,
			ID3D12RootSignature* rootSignature
		);
	};
}
