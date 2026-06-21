#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "PipelineCache.h"

namespace Unnamed::Render {
	class RenderDevice;

	/// @brief パイプラインハンドル種別。
	enum class PIPELINE_KIND : uint8_t {
		GRAPHICS = 0,
		COMPUTE  = 1,
	};

	/// @brief パイプライン登録エントリを識別するハンドル。
	struct PipelineHandle {
		static constexpr uint32_t kInvalidIndex = 0xFFFF'FFFFu;

		PIPELINE_KIND kind  = PIPELINE_KIND::GRAPHICS;
		uint32_t      index = kInvalidIndex;

		/// @brief ハンドルが有効かどうかを判定します。
		/// @return 有効ならtrue
		[[nodiscard]] bool IsValid() const {
			return index != kInvalidIndex;
		}
	};

	/// @brief グラフィクスパイプラインの登録仕様。
	struct GraphicsPipelineSpec {
		std::string          debugName;
		AssetID              shaderProgramId = kInvalidAssetID;
		ID3D12RootSignature* rootSignature   = nullptr;
		GraphicsPsoKey       psoTemplate     = {};
	};

	/// @brief コンピュートパイプラインの登録仕様。
	struct ComputePipelineSpec {
		std::string          debugName;
		AssetID              shaderProgramId = kInvalidAssetID;
		ID3D12RootSignature* rootSignature   = nullptr;
		ComputePipelineKey   psoTemplate     = {};
	};

	/// @brief 解決済みグラフィクスパイプライン。
	struct ResolvedGraphicsPipeline {
		ID3D12RootSignature* rootSignature = nullptr;
		ID3D12PipelineState* pso           = nullptr;
	};

	/// @brief 解決済みコンピュートパイプライン。
	struct ResolvedComputePipeline {
		ID3D12RootSignature* rootSignature = nullptr;
		ID3D12PipelineState* pso           = nullptr;
	};

	/// @brief パイプライン仕様の登録とPSO解決を管理するレジストリ。
	class PipelineRegistry {
	public:
		/// @brief グラフィクスパイプラインを登録します。
		/// @param spec 登録仕様
		/// @return 登録済みハンドル
		PipelineHandle RegisterGraphics(const GraphicsPipelineSpec& spec);

		/// @brief コンピュートパイプラインを登録します。
		/// @param spec 登録仕様
		/// @return 登録済みハンドル
		PipelineHandle RegisterCompute(const ComputePipelineSpec& spec);

		/// @brief すべての登録済みパイプラインを消去します。
		void Clear();

		/// @brief 登録済み仕様をもとにPSOを解決します。
		/// @param renderDevice 描画デバイス
		void ResolveAll(RenderDevice& renderDevice);

		/// @brief 解決済みグラフィクスパイプラインを取得します。
		/// @param handle パイプラインハンドル
		/// @return 解決済み情報。無効時はnullptr
		[[nodiscard]] const ResolvedGraphicsPipeline* GetGraphics(
			PipelineHandle handle
		) const;

		/// @brief 解決済みコンピュートパイプラインを取得します。
		/// @param handle パイプラインハンドル
		/// @return 解決済み情報。無効時はnullptr
		[[nodiscard]] const ResolvedComputePipeline* GetCompute(
			PipelineHandle handle
		) const;

	private:
		struct GraphicsEntry {
			GraphicsPipelineSpec     spec     = {};
			ResolvedGraphicsPipeline resolved = {};
		};

		struct ComputeEntry {
			ComputePipelineSpec     spec     = {};
			ResolvedComputePipeline resolved = {};
		};

		std::vector<GraphicsEntry> mGraphics = {};
		std::vector<ComputeEntry>  mCompute  = {};
	};
}
