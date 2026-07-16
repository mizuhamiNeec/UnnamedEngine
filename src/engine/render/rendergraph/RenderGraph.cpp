#include "RenderGraph.h"

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

#define USE_PIX_MARKERS

static void BeginGpuEvent(ID3D12GraphicsCommandList* cl, const char* name) {
#if defined(USE_PIX_MARKERS)
	PIXBeginEvent(cl, 0, name);
#else
	(void)cl;
	(void)name;
#endif
}

static void EndGpuEvent(ID3D12GraphicsCommandList* cl) {
#if defined(USE_PIX_MARKERS)
	PIXEndEvent(cl);
#else
	(void)cl;
#endif
}

namespace Unnamed::Render {
	static D3D12_RESOURCE_STATES DefaultInitState(const uint32_t id) {
		return id == RenderGraph::kBackBufferId ?
			       D3D12_RESOURCE_STATE_PRESENT :
			       D3D12_RESOURCE_STATE_COMMON;
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

		const auto ensureTrackedResourceState =
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

		for (
			uint32_t passIndex = 0;
			passIndex < static_cast<uint32_t>(mPasses.size());
			++passIndex
		) {
			const auto&  pass     = mPasses[passIndex];
			CompiledPass compiled = {};
			compiled.passIndex    = passIndex;

			compiled.colorRts = pass.colorRts;
			compiled.depthRt  = pass.depthRt;

			std::unordered_map<uint32_t, D3D12_RESOURCE_STATES> requiredStates;

			// 出力ターゲットで要求を確定
			for (uint32_t rtId : pass.colorRts) {
				requiredStates[rtId] = D3D12_RESOURCE_STATE_RENDER_TARGET;
			}
			if (pass.depthRt.has_value()) {
				requiredStates[*pass.depthRt] =
					D3D12_RESOURCE_STATE_DEPTH_WRITE;
			}

			// 使用するテクスチャの要求状態を確定
			for (const auto& use : pass.uses) {
				auto& req = requiredStates[use.textureId];

				switch (use.access) {
					case RG_ACCESS::SRV_READ_PS: {
						// UAV/RT/DSVと同居はヤヴァい
						if (
							req == D3D12_RESOURCE_STATE_RENDER_TARGET ||
							req == D3D12_RESOURCE_STATE_UNORDERED_ACCESS ||
							req == D3D12_RESOURCE_STATE_DEPTH_WRITE
						) {
							Fatal(
								"RDG",
								"パス名 '{}' に textureID={} のRT/DSV/UAV使用とSRV(PS)読み取りが同時に指定されています。",
								pass.name, use.textureId
							);
							UASSERT(false);
							break;
						}
						req |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
						break;
					}

					case RG_ACCESS::SRV_READ_CS: {
						if (
							req == D3D12_RESOURCE_STATE_RENDER_TARGET ||
							req == D3D12_RESOURCE_STATE_UNORDERED_ACCESS ||
							req == D3D12_RESOURCE_STATE_DEPTH_WRITE
						) {
							Fatal(
								"RDG",
								"パス名 '{}' に textureID={} のRT/DSV/UAV使用とSRV(CS)読み取りが同時に指定されています。",
								pass.name, use.textureId
							);
							UASSERT(false);
							break;
						}
						req |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
						break;
					}

					case RG_ACCESS::UAV_WRITE: {
						// すでにRT/DSVならヤヴァい
						if (
							req == D3D12_RESOURCE_STATE_RENDER_TARGET ||
							req == D3D12_RESOURCE_STATE_DEPTH_WRITE
						) {
							Fatal(
								"RDG",
								"パス名 '{}' に textureID={} のRT/DSV使用とUAV書き込みが同時に指定されています。",
								pass.name, use.textureId
							);
							UASSERT(false);
							break;
						}
						req = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
						compiled.uavWritesInPass.emplace_back(use.textureId);
						break;
					}

					case RG_ACCESS::DEPTH_READ: {
						// TODO: 今のところ使わないので未対応。
						break;
					}

					case RG_ACCESS::RENDER_TARGET: break;
					case RG_ACCESS::DEPTH_WRITE: break;
						// こいつらは無視
				}
			}

			auto HasRtUse = [&](uint32_t texId) {
				return std::ranges::contains(pass.colorRts, texId);
			};

			for (const auto& c : pass.clearsColor) {
				if (!HasRtUse(c.textureId)) {
					Fatal(
						"RDG",
						"パス名 '{}' のクリアコマンドで textureID={} が指定されていますが、このパスでRT使用がありません。",
						pass.name, c.textureId
					);
				}
				compiled.clearsBefore.push_back(
					RgClearCmd{
						.textureId = c.textureId,
						.color     = Color{
							c.color.r, c.color.g, c.color.b, c.color.a
						},
					}
				);
			}
			for (const auto& dc : pass.clearDepth) {
				compiled.clearDepthBefore.emplace_back(
					RgDepthClearCmd{
						.textureId = dc.textureId,
						.depth     = dc.depth,
						.stencil   = dc.stencil,
					}
				);
			}

			for (const auto& use : pass.uses) {
				if (IsSrvRead(use.access) && uavWrittenPendingBarrier.
				    contains(use.textureId)) {
					compiled.uavBarriersBefore.emplace_back(use.textureId);
					uavWrittenPendingBarrier.erase(use.textureId);
				}
			}

			for (const auto& [textureId, req] : requiredStates) {
				if (req == 0) {
					continue;
				}

				ensureTrackedResourceState(textureId);

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
		mIsDirty           = true;
	}

	void RenderGraph::Reset() {
		mPasses.clear();
		mCompiled.clear();
		mIsDirty = true;
	}

	void RenderGraph::BeginPass(
		Rhi::IRhiDevice&           device, Rhi::D3D12CommandContext& context,
		ID3D12GraphicsCommandList* commandList, RenderPassContext& passContext,
		const char*                passName,
		const CompiledPass&        cp
	) {
		// 遷移
		for (const auto& tr : cp.transitionsBefore) {
			ID3D12Resource* res = ResolveResource(device, tr.textureId);
			if (!res) {
				Fatal("RDG", "リソースの解決に失敗しました: textureId={}", tr.textureId);
				UASSERT(false);
				continue;
			}

			const auto curIt = mGlobalStates.find(tr.textureId);
			const D3D12_RESOURCE_STATES trackedBefore = curIt != mGlobalStates.
					end() ?
					curIt->second :
					DefaultInitState(
						tr.textureId
					);
			if (trackedBefore != tr.before) {
				DevMsg(
					"RDG",
					"State drift detected in pass='{}' for textureId={}: trackedBefore=0x{:X}, compiledBefore=0x{:X}, after=0x{:X}",
					passName ? passName : "<null>",
					tr.textureId,
					static_cast<uint32_t>(trackedBefore),
					static_cast<uint32_t>(tr.before),
					static_cast<uint32_t>(tr.after)
				);
			}

			context.TransitionResource(res, tr.before, tr.after);
			mGlobalStates[tr.textureId] = tr.after;
		}

		// UAVのバリア
		for (const uint32_t id : cp.uavBarriersBefore) {
			ID3D12Resource* res = ResolveResource(device, id);
			if (!res) {
				Fatal("RDG", "リソースの解決に失敗しました(UAV barrier): textureId={}", id);
				UASSERT(false);
				continue;
			}

			D3D12_RESOURCE_BARRIER uav = {};
			uav.Type                   = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			uav.UAV.pResource          = res;
			commandList->ResourceBarrier(1, &uav);
		}

		// RTとDSのセット
		if (!cp.colorRts.empty() || cp.depthRt.has_value()) {
			passContext.SetRenderTargetAndDepth(cp.colorRts, cp.depthRt);
		}

		// クリア
		for (const auto& c : cp.clearsBefore) {
			passContext.ClearColorById(
				c.textureId, c.color.r, c.color.g, c.color.b, c.color.a
			);
		}

		// DEPTHクリア
		for (auto& d : cp.clearDepthBefore) {
			passContext.ClearDepthStencilById(d.textureId, d.depth, d.stencil);
		}
	}

	void RenderGraph::EndPass(RenderPassContext&, const CompiledPass&) {
		// 将来使います
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

	bool RenderGraph::IsSrvRead(const RG_ACCESS access) {
		return access == RG_ACCESS::SRV_READ_PS || access ==
		       RG_ACCESS::SRV_READ_CS;
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

	void RenderGraph::ExecutePasses(Rhi::IRhiDevice&) {
	}
}
