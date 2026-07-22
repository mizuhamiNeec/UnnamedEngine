#pragma once
#include <cstdint>
#include <d3d12.h>
#include <functional>
#include <string>
#include <unordered_map>

#include "RenderGraphBuilder.h"

#include "engine/render/RenderDevice.h"

namespace Unnamed::Rhi {
	class D3D12CommandContext;
	class IRhiDevice;
}

namespace Unnamed::Render {
	enum class RG_ACCESS : uint8_t;
	struct RgUse;
	class RenderGraphBuilder;
	class RenderPassContext;

	struct RgPass {
		std::string                  name;
		std::vector<RgUse>           uses;
		std::vector<RgClearCmd>      clearsColor;
		std::vector<RgDepthClearCmd> clearDepth;

		std::vector<uint32_t>   colorRts;
		std::optional<uint32_t> depthRt;

		std::function<void(RenderPassContext&)> execute;
	};

	struct CompiledTransition {
		uint32_t              textureId = 0;
		D3D12_RESOURCE_STATES before    = D3D12_RESOURCE_STATE_COMMON;
		D3D12_RESOURCE_STATES after     = D3D12_RESOURCE_STATE_COMMON;
	};

	struct CompiledPass {
		std::vector<CompiledTransition> transitionsBefore;
		std::vector<uint32_t>           uavBarriersBefore;
		std::vector<uint32_t>           uavWritesInPass;

		std::vector<uint32_t>   colorRts;
		std::optional<uint32_t> depthRt;

		std::vector<RgClearCmd>      clearsBefore;
		std::vector<RgDepthClearCmd> clearDepthBefore;

		uint32_t passIndex = 0;
	};

	enum class GRAPH_RESOURCE_STATE : uint8_t {
		UNKNOWN,
		PRESENT,
		RENDER_TARGET,
	};

	/// @brief パスのリソース契約から実行順と D3D12 バリアをコンパイルします。
	/// @details テクスチャの所有権と寿命は RgResourceRegistry にあり、RenderGraph は
	///          フレーム内の依存関係だけを管理します。
	class RenderGraph {
	public:
		static constexpr uint32_t kBackBufferId = 0xFFFF'FFFEu;

		/// @brief リソースレジストリを所有する描画デバイスを設定します。
		void SetRenderDevice(RenderDevice& renderDevice);

		/// @brief Registry 所有の永続テクスチャを作成します。解放は作成者の責務です。
		[[nodiscard]] uint32_t CreateTexture(const RgTextureDesc& desc) const;

		/// @brief リソース read/write 契約と実行コールバックを持つパスを登録します。
		/// @details setup はアタッチメントとアクセスを宣言するだけで、execute はそれを変更しません。
		void AddPass(
			std::string                                     name,
			const std::function<void(RenderGraphBuilder&)>& setup,
			std::function<void(RenderPassContext&)>         execute
		);

		/// @brief 宣言されたアクセスから依存順、バリア、アタッチメント、クリアを確定します。
		void Compile(Rhi::IRhiDevice& device);

		/// @brief コンパイル済みのパスを実行順に記録し、各パスのアタッチメントを設定します。
		void Execute(Rhi::IRhiDevice& device);

		/// @brief 追跡中のリソース状態を破棄し、次回実行前の再コンパイルを要求します。
		void Invalidate();
		/// @brief フレーム内パスを破棄し、次フレーム用の空グラフへ戻します。
		void Reset();

	private:
		void BeginPass(
			Rhi::IRhiDevice&           device,
			Rhi::D3D12CommandContext&  context,
			ID3D12GraphicsCommandList* commandList,
			const RenderPassContext&   passContext,
			const char*                passName,
			const CompiledPass&        cp
		);

		void EndPass(RenderPassContext& passContext, const CompiledPass& cp);

		static D3D12_RESOURCE_STATES RequiredState(RG_ACCESS access);
		static bool                  IsUavWrite(RG_ACCESS access);
		[[nodiscard]] bool HasCachedDependencyTopology() const;
		void CacheDependencyTopology(
			const std::vector<uint32_t>& executionOrder
		);

		ID3D12Resource* ResolveResource(
			Rhi::IRhiDevice& device, uint32_t id
		) const;

		RenderDevice* mRenderDevice = nullptr;

		std::vector<RgPass> mPasses;
		std::unordered_map<uint32_t, D3D12_RESOURCE_STATES>
		mGlobalStates;
		std::unordered_map<uint32_t, uint64_t> mKnownResourceRevisions;

		std::vector<CompiledPass> mCompiled;
		/// @brief 同一の pass/use 列に対して再利用する依存グラフの実行順です。
		/// @details バリアとアタッチメントはリソース世代・状態に依存するため、毎フレーム再コンパイルします。
		std::vector<std::vector<RgUse>> mCachedDependencyUses;
		std::vector<uint32_t>           mCachedExecutionOrder;
		bool                      mIsDirty           = true;
		bool                      mStatesInitialized = false;
	};
}
