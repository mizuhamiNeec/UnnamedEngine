#include "RenderGraph.h"

#include <algorithm>
#include <pix.h>
#include <unordered_set>
#include <utility>

#include "RegistryDescriptorResolver.h"
#include "RenderGraphBuilder.h"
#include "RenderPassContext.h"

#include "core/UnnamedMacro.h"

#include "engine/rhi/d3d12/D3D12CommandContext.h"
#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/rhi/d3d12/D3D12SwapChain.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace {
#define USE_PIX_MARKERS // PIXマーカー

	void BeginGpuEvent(ID3D12GraphicsCommandList* cl, const char* name) {
#if defined(USE_PIX_MARKERS)
		PIXBeginEvent(cl, 0, name);
#else
		(void)cl;
		(void)name;
#endif
	}

	void EndGpuEvent(ID3D12GraphicsCommandList* cl) {
#if defined(USE_PIX_MARKERS)
		PIXEndEvent(cl);
#else
		(void)cl;
#endif
	}
}

namespace Unnamed::Render {
	static constexpr std::string_view kChannel = "RDG";

	namespace {
		struct PassResourceAccess {
			bool isRenderTarget = false;
			bool isUavWrite     = false;
			bool isSrvRead      = false;
			bool isDepthWrite   = false;
			bool isDepthRead    = false;

			[[nodiscard]] bool HasRead() const {
				return isSrvRead || isDepthRead;
			}

			[[nodiscard]] bool HasWrite() const {
				return isRenderTarget || isUavWrite || isDepthWrite;
			}
		};

		struct ResourceDependencyState {
			std::optional<uint32_t> lastWriter;
			std::vector<uint32_t>   readersSinceLastWrite;
		};

		void AddDependency(
			std::vector<std::vector<uint32_t>>& outgoingDependencies,
			const uint32_t                      beforePassIndex,
			const uint32_t                      afterPassIndex
		) {
			if (beforePassIndex == afterPassIndex) {
				return;
			}

			auto& dependents = outgoingDependencies[beforePassIndex];
			if (!std::ranges::contains(dependents, afterPassIndex)) {
				dependents.emplace_back(afterPassIndex);
			}
		}

		D3D12_RESOURCE_STATES DefaultInitState(const uint32_t id) {
			return id == RenderGraph::kBackBufferId ?
				       D3D12_RESOURCE_STATE_PRESENT :
				       D3D12_RESOURCE_STATE_COMMON;
		}
	}

	void RenderGraph::SetRenderDevice(RenderDevice& renderDevice) {
		mRenderDevice = &renderDevice;
	}

	uint32_t RenderGraph::CreateTexture(const RgTextureDesc& desc) const {
		return mRenderDevice->GetRegistry().CreateTexture(desc);
	}

	void RenderGraph::AddPass(
		std::string                                     name,
		const std::function<void(RenderGraphBuilder&)>& setup,
		std::function<void(RenderPassContext&)>         execute
	) {
		RenderGraphBuilder builder(mRenderDevice->GetRegistry());
		setup(builder);

		RgPass pass = {};
		pass.name   = std::move(name);
		pass.uses   = builder.GetUses();

		pass.clearsColor = builder.GetClearColors();
		pass.clearDepth  = builder.GetClearDepths();

		pass.colorRts = builder.GetColorRts();
		pass.depthRt  = builder.GetDepthRt();

		pass.execute = std::move(execute);

		mPasses.emplace_back(std::move(pass));
		mIsDirty = true;
	}

	void RenderGraph::Compile(Rhi::IRhiDevice&) {
		if (!mIsDirty) {
			return;
		}

		mCompiled.clear();
		mCompiled.reserve(mPasses.size());

		if (!mStatesInitialized) {
			mGlobalStates.clear();
			mKnownResourceRevisions.clear();
			mGlobalStates[kBackBufferId] = D3D12_RESOURCE_STATE_PRESENT;
			mKnownResourceRevisions[kBackBufferId] = 0;
			mStatesInitialized = true;
		}

		std::unordered_map<uint32_t, D3D12_RESOURCE_STATES> plannedStates =
			mGlobalStates;

		// BackBufferは毎フレームPresent状態から開始する
		plannedStates[kBackBufferId] = D3D12_RESOURCE_STATE_PRESENT;
		mGlobalStates[kBackBufferId] = D3D12_RESOURCE_STATE_PRESENT;

		std::vector<uint32_t> executionOrder;
		if (HasCachedDependencyTopology()) {
			executionOrder = mCachedExecutionOrder;
		} else {
			// パス登録順とは別に、宣言済みリソースアクセスから RAW/WAR/WAW 依存を
			// 構築する。これにより実行順は手続き的な追加順ではなく、リソース契約に
			// よって決まる。複数の有効順がある場合だけ登録順を維持する。
			std::vector<std::vector<uint32_t>> outgoingDependencies(mPasses.size());
			std::unordered_map<uint32_t, ResourceDependencyState> resourceDependencyStates;
			for (uint32_t passIndex = 0; passIndex < static_cast<uint32_t>(mPasses.size()); ++passIndex) {
				std::unordered_map<uint32_t, PassResourceAccess> accesses;
				for (const RgUse& use : mPasses[passIndex].uses) {
					auto& access = accesses[use.textureId];
					switch (use.access) {
						case RG_ACCESS::RENDER_TARGET: access.isRenderTarget = true; break;
						case RG_ACCESS::UAV_WRITE: access.isUavWrite = true; break;
						case RG_ACCESS::SRV_READ_PS:
						case RG_ACCESS::SRV_READ_CS: access.isSrvRead = true; break;
						case RG_ACCESS::DEPTH_WRITE: access.isDepthWrite = true; break;
						case RG_ACCESS::DEPTH_READ: access.isDepthRead = true; break;
					}
				}
				for (const auto& [textureId, access] : accesses) {
					auto& dependencyState = resourceDependencyStates[textureId];
					if (access.HasRead()) {
						if (dependencyState.lastWriter.has_value()) {
							AddDependency(outgoingDependencies, *dependencyState.lastWriter, passIndex);
						}
						dependencyState.readersSinceLastWrite.emplace_back(passIndex);
					}
					if (access.HasWrite()) {
						if (dependencyState.lastWriter.has_value()) {
							AddDependency(outgoingDependencies, *dependencyState.lastWriter, passIndex);
						}
						for (const uint32_t readerPassIndex : dependencyState.readersSinceLastWrite) {
							AddDependency(outgoingDependencies, readerPassIndex, passIndex);
						}
						dependencyState.readersSinceLastWrite.clear();
						dependencyState.lastWriter = passIndex;
					}
				}
			}
			std::vector<uint32_t> inDegree(mPasses.size());
			for (const auto& dependents : outgoingDependencies) {
				for (const uint32_t dependentPassIndex : dependents) {
					++inDegree[dependentPassIndex];
				}
			}
			std::vector<uint32_t> readyPasses;
			readyPasses.reserve(mPasses.size());
			for (uint32_t passIndex = 0; passIndex < static_cast<uint32_t>(mPasses.size()); ++passIndex) {
				if (inDegree[passIndex] == 0) {
					readyPasses.emplace_back(passIndex);
				}
			}
			executionOrder.reserve(mPasses.size());
			while (!readyPasses.empty()) {
				const auto nextIt = std::ranges::min_element(readyPasses);
				const uint32_t passIndex = *nextIt;
				readyPasses.erase(nextIt);
				executionOrder.emplace_back(passIndex);
				for (const uint32_t dependentPassIndex : outgoingDependencies[passIndex]) {
					if (--inDegree[dependentPassIndex] == 0) {
						readyPasses.emplace_back(dependentPassIndex);
					}
				}
			}
			if (executionOrder.size() != mPasses.size()) {
				Fatal(kChannel, "RenderGraph pass dependencies contain a cycle.");
				UASSERT(false);
				return;
			}
			CacheDependencyTopology(executionOrder);
		}

		const auto EnsureTrackedResourceState =
			[this, &plannedStates](const uint32_t textureId) {
			if (textureId == kBackBufferId) {
				return;
			}

			const uint64_t currentRevision = mRenderDevice->GetRegistry().
				GetResourceRevision(textureId);
			const auto revIt = mKnownResourceRevisions.find(textureId);
			if (
				revIt != mKnownResourceRevisions.end() &&
				revIt->second == currentRevision
			) {
				return;
			}

			// 再生成されたリソースに前世代の状態遷移履歴を適用しない
			const D3D12_RESOURCE_STATES resetState =
				DefaultInitState(textureId);
			plannedStates[textureId]           = resetState;
			mGlobalStates[textureId]           = resetState;
			mKnownResourceRevisions[textureId] = currentRevision;
		};

		std::unordered_set<uint32_t> uavWrittenPendingBarrier;

		for (const uint32_t passIndex : executionOrder) {
			const auto&  pass     = mPasses[passIndex];
			CompiledPass compiled = {};
			compiled.passIndex    = passIndex;

			compiled.colorRts = pass.colorRts;
			compiled.depthRt  = pass.depthRt;

			std::unordered_map<uint32_t, PassResourceAccess>    accesses;
			std::unordered_map<uint32_t, D3D12_RESOURCE_STATES> requiredStates;
			for (const RgUse& use : pass.uses) {
				auto& access = accesses[use.textureId];
				switch (use.access) {
					case RG_ACCESS::RENDER_TARGET: access.isRenderTarget = true;
						break;
					case RG_ACCESS::UAV_WRITE: access.isUavWrite = true;
						break;
					case RG_ACCESS::SRV_READ_PS:
					case RG_ACCESS::SRV_READ_CS: access.isSrvRead = true;
						break;
					case RG_ACCESS::DEPTH_WRITE: access.isDepthWrite = true;
						break;
					case RG_ACCESS::DEPTH_READ: access.isDepthRead = true;
						break;
				}
				requiredStates[use.textureId] |= RequiredState(use.access);
			}

			bool hasInvalidAccess = false;
			for (const auto& [textureId, access] : accesses) {
				const uint32_t writeAccessCount =
					static_cast<uint32_t>(access.isRenderTarget) +
					static_cast<uint32_t>(access.isUavWrite) +
					static_cast<uint32_t>(access.isDepthWrite);
				if (
					writeAccessCount > 1 ||
					(access.HasRead() && access.HasWrite()) ||
					access.isDepthRead
				) {
					Fatal(
						kChannel,
						"Pass '{}' declares unsupported or incompatible access for textureId={}",
						pass.name,
						textureId
					);
					hasInvalidAccess = true;
				}
			}
			if (hasInvalidAccess) {
				UASSERT(false);
				return;
			}

			for (const auto& [textureId, access] : pass.uses) {
				if (IsUavWrite(access)) {
					compiled.uavWritesInPass.emplace_back(textureId);
				}
			}

			auto HasRtUse = [&](const uint32_t texId) {
				return std::ranges::contains(pass.colorRts, texId);
			};

			for (const auto& [textureId, color] : pass.clearsColor) {
				if (!HasRtUse(textureId)) {
					Fatal(
						kChannel,
						"パス名 '{}' のクリアコマンドで textureID={} が指定されていますが、このパスでRT使用がありません。",
						pass.name, textureId
					);
					UASSERT(false);
					return;
				}
				compiled.clearsBefore.push_back(
					RgClearCmd{
						.textureId = textureId,
						.color     = {
							.r = color.r, .g = color.g, .b = color.b,
							.a = color.a
						},
					}
				);
			}
			for (const auto& [textureId, depth, stencil] : pass.clearDepth) {
				if (!pass.depthRt.has_value() || *pass.depthRt != textureId) {
					Fatal(
						kChannel,
						"Pass '{}' clears textureId={} as depth without declaring it as the depth target.",
						pass.name,
						textureId
					);
					UASSERT(false);
					return;
				}
				compiled.clearDepthBefore.emplace_back(
					RgDepthClearCmd{
						.textureId = textureId,
						.depth     = depth,
						.stencil   = stencil,
					}
				);
			}

			for (const auto& [textureId, access] : pass.uses) {
				if (uavWrittenPendingBarrier.contains(textureId)) {
					compiled.uavBarriersBefore.emplace_back(textureId);
					uavWrittenPendingBarrier.erase(textureId);
				}
			}

			for (const auto& [textureId, req] : requiredStates) {
				if (req == 0) {
					continue;
				}

				EnsureTrackedResourceState(textureId);

				// plannedStates になければ初期状態にする
				auto it = plannedStates.find(textureId);
				if (it == plannedStates.end()) {
					plannedStates.emplace(
						textureId, DefaultInitState(textureId)
					);
					it = plannedStates.find(textureId);
				}

				auto& cur = it->second;

				if (cur != req) {
					compiled.transitionsBefore.emplace_back(
						CompiledTransition{
							.textureId = textureId,
							.before    = cur,
							.after     = req
						}
					);

					cur = req;
				}
			}

			// このパスでUAV書き込みがある場合は 保留状態にしておく
			for (uint32_t id : compiled.uavWritesInPass) {
				uavWrittenPendingBarrier.insert(id);
			}

			mCompiled.emplace_back(std::move(compiled));
		}
		mIsDirty = false;
	}

	void RenderGraph::Execute(Rhi::IRhiDevice& device) {
		Compile(device);

		const auto& dxDevice    = dynamic_cast<Rhi::D3D12Device&>(device);
		auto*       commandList = dxDevice.GetCommandList();
		auto*       swapChain   = dxDevice.GetD3D12SwapChain();

		Rhi::D3D12CommandContext context(commandList, swapChain);
		context.Begin();

		const uint32_t w = dxDevice.GetD3D12SwapChain()->GetWidth();
		const uint32_t h = dxDevice.GetD3D12SwapChain()->GetHeight();

		const RegistryDescriptorResolver resolver(mRenderDevice->GetRegistry());

		RenderPassContext passContext(
			context,
			commandList,
			w,
			h,
			dxDevice.GetSrvUavHeap(),
			resolver
		);

		// コンパイル済みの遷移とバリアを各パス実行直前に適用する
		for (const auto& cp : mCompiled) {
			const auto& pass = mPasses[cp.passIndex];
			BeginGpuEvent(commandList, pass.name.c_str());

			BeginPass(
				device,
				context,
				commandList,
				passContext,
				pass.name.c_str(),
				cp
			);
			pass.execute(passContext);
			EndPass(passContext, cp);

			EndGpuEvent(commandList);
		}

		// 次フレームと Present の契約を満たすため、バックバッファ状態を戻す
		{
			ID3D12Resource* bb  = ResolveResource(device, kBackBufferId);
			auto&           cur = mGlobalStates[kBackBufferId];
			if (cur != D3D12_RESOURCE_STATE_PRESENT) {
				context.TransitionResource(
					bb, cur, D3D12_RESOURCE_STATE_PRESENT
				);
				cur = D3D12_RESOURCE_STATE_PRESENT;
			}
		}

		context.End();
	}

	void RenderGraph::Invalidate() {
		mGlobalStates.clear();
		mKnownResourceRevisions.clear();
		mStatesInitialized = false;
		mCachedDependencyUses.clear();
		mCachedExecutionOrder.clear();
		mIsDirty           = true;
	}

	void RenderGraph::Reset() {
		mPasses.clear();
		mCompiled.clear();
		// pass は毎フレーム構築し直すが、同じリソース契約なら依存順は再利用できる。
		mIsDirty = true;
	}

	bool RenderGraph::HasCachedDependencyTopology() const {
		if (
			mCachedDependencyUses.size() != mPasses.size() ||
			mCachedExecutionOrder.size() != mPasses.size()
		) {
			return false;
		}
		for (size_t passIndex = 0; passIndex < mPasses.size(); ++passIndex) {
			const auto& cachedUses = mCachedDependencyUses[passIndex];
			const auto& currentUses = mPasses[passIndex].uses;
			if (cachedUses.size() != currentUses.size()) {
				return false;
			}
			for (size_t useIndex = 0; useIndex < currentUses.size(); ++useIndex) {
				if (
					cachedUses[useIndex].textureId != currentUses[useIndex].textureId ||
					cachedUses[useIndex].access != currentUses[useIndex].access
				) {
					return false;
				}
			}
		}
		return true;
	}

	void RenderGraph::CacheDependencyTopology(
		const std::vector<uint32_t>& executionOrder
	) {
		mCachedDependencyUses.clear();
		mCachedDependencyUses.reserve(mPasses.size());
		for (const RgPass& pass : mPasses) {
			mCachedDependencyUses.emplace_back(pass.uses);
		}
		mCachedExecutionOrder = executionOrder;
	}

	void RenderGraph::BeginPass(
		Rhi::IRhiDevice&           device, Rhi::D3D12CommandContext& context,
		ID3D12GraphicsCommandList* commandList,
		const RenderPassContext&   passContext,
		const char*                passName,
		const CompiledPass&        cp
	) {
		// 遷移
		for (const auto& [textureId, before, after] : cp.transitionsBefore) {
			ID3D12Resource* res = ResolveResource(device, textureId);
			if (!res) {
				Fatal(kChannel, "リソースの解決に失敗しました: textureId={}", textureId);
				UASSERT(false);
				continue;
			}

			const auto curIt = mGlobalStates.find(textureId);
			const D3D12_RESOURCE_STATES trackedBefore = curIt != mGlobalStates.
					end() ?
					curIt->second :
					DefaultInitState(
						textureId
					);
			if (trackedBefore != before) {
				DevMsg(
					kChannel,
					"State drift detected in pass='{}' for textureId={}: trackedBefore=0x{:X}, compiledBefore=0x{:X}, after=0x{:X}",
					passName ? passName : "<null>",
					textureId,
					static_cast<uint32_t>(trackedBefore),
					static_cast<uint32_t>(before),
					static_cast<uint32_t>(after)
				);
			}

			context.TransitionResource(res, before, after);
			mGlobalStates[textureId] = after;
		}

		// UAVのバリア
		for (const uint32_t id : cp.uavBarriersBefore) {
			ID3D12Resource* res = ResolveResource(device, id);
			if (!res) {
				Fatal(kChannel, "リソースの解決に失敗しました(UAV barrier): textureId={}", id);
				UASSERT(false);
				continue;
			}

			D3D12_RESOURCE_BARRIER uav = {};
			uav.Type                   = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			uav.UAV.pResource          = res;
			commandList->ResourceBarrier(1, &uav);
		}

		// setup で確定したアタッチメントを一度だけ設定する。パスコールバックはこれを変更しない。
		if (!cp.colorRts.empty() || cp.depthRt.has_value()) {
			passContext.SetRenderTargetAndDepth(cp.colorRts, cp.depthRt);
		}

		// クリア
		for (const auto& [textureId, color] : cp.clearsBefore) {
			passContext.ClearColorById(
				textureId, color.r, color.g, color.b, color.a
			);
		}

		// DEPTHクリア
		for (const auto& [textureId, depth, stencil] : cp.clearDepthBefore) {
			passContext.ClearDepthStencilById(textureId, depth, stencil);
		}
	}

	void RenderGraph::EndPass(RenderPassContext&, const CompiledPass&) {
		// TODO: 将来使います
	}

	D3D12_RESOURCE_STATES RenderGraph::RequiredState(const RG_ACCESS access) {
		switch (access) {
			case RG_ACCESS::RENDER_TARGET: return
					D3D12_RESOURCE_STATE_RENDER_TARGET;
			case RG_ACCESS::UAV_WRITE: return
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			case RG_ACCESS::SRV_READ_PS: return
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			case RG_ACCESS::SRV_READ_CS: return
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			case RG_ACCESS::DEPTH_WRITE: return
					D3D12_RESOURCE_STATE_DEPTH_WRITE;
			case RG_ACCESS::DEPTH_READ: return D3D12_RESOURCE_STATE_DEPTH_READ;
			default: return D3D12_RESOURCE_STATE_COMMON;
		}
	}

	bool RenderGraph::IsUavWrite(const RG_ACCESS access) {
		return access == RG_ACCESS::UAV_WRITE;
	}

	ID3D12Resource* RenderGraph::ResolveResource(
		Rhi::IRhiDevice& device, const uint32_t id
	) const {
		const auto& dxDevice = dynamic_cast<Rhi::D3D12Device&>(device);

		if (id == kBackBufferId) {
			const auto*    swapChain = dxDevice.GetD3D12SwapChain();
			const uint32_t bbIndex   = swapChain->GetCurrentBackBufferIndex();
			return swapChain->GetBackBuffer(bbIndex);
		}

		return mRenderDevice->GetRegistry().GetResource(id);
	}
}
