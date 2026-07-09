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

	/// @brief 起動時にPipelineを解決する必要性です。
	enum class PIPELINE_STARTUP_REQUIREMENT : uint8_t {
		REQUIRED,
		CONFIGURED_OPTIONAL,
	};

	/// @brief 一括Pipeline解決の対象範囲です。
	enum class PIPELINE_RESOLVE_SCOPE : uint8_t {
		REQUIRED_ONLY,
		ALL_REGISTERED,
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
		PIPELINE_STARTUP_REQUIREMENT startupRequirement =
			PIPELINE_STARTUP_REQUIREMENT::REQUIRED;
	};

	/// @brief コンピュートパイプラインの登録仕様。
	struct ComputePipelineSpec {
		std::string          debugName;
		AssetID              shaderProgramId = kInvalidAssetID;
		ID3D12RootSignature* rootSignature   = nullptr;
		ComputePipelineKey   psoTemplate     = {};
		PIPELINE_STARTUP_REQUIREMENT startupRequirement =
			PIPELINE_STARTUP_REQUIREMENT::REQUIRED;
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

	/// @brief 登録済みPipelineの一括解決結果です。
	struct PipelineResolveResult final {
		uint32_t requestedCount   = 0;
		uint32_t resolvedCount    = 0;
		uint32_t newlyFailedCount = 0;

		/// @brief 全Pipelineが解決されたか判定します。
		/// @return 全件成功した場合true。
		[[nodiscard]] bool Succeeded() const noexcept {
			return requestedCount == resolvedCount;
		}
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
		[[nodiscard]] PipelineResolveResult ResolveAll(
			RenderDevice& renderDevice,
			PIPELINE_RESOLVE_SCOPE scope =
				PIPELINE_RESOLVE_SCOPE::ALL_REGISTERED
		);

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
			bool                     resolveFailureLogged = false;
		};

		struct ComputeEntry {
			ComputePipelineSpec     spec     = {};
			ResolvedComputePipeline resolved = {};
			bool                    resolveFailureLogged = false;
		};

		std::vector<GraphicsEntry> mGraphics = {};
		std::vector<ComputeEntry>  mCompute  = {};
	};
}
