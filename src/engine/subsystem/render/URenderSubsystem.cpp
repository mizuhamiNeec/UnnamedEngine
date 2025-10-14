#include <pch.h>
#include <unordered_set>

#include <engine/gameframework/component/MeshRenderer/MeshRendererComponent.h>
#include <engine/gameframework/component/Transform/TransformComponent.h>
#include <engine/gameframework/world/UWorld.h>
#include <engine/subsystem/interface/ServiceLocator.h>
#include <engine/subsystem/render/URenderSubsystem.h>

#include "engine/uuploadarena/UploadArena.h"

namespace Unnamed {
	namespace {
		float ComputeViewDepth(const Mat4& world, const Mat4& view) {
			const Vec4 worldPos = Vec4(world.m[3][0], world.m[3][1],
			                           world.m[3][2], 1.0f);
			const Vec4 viewPos = worldPos * view;
			return viewPos.z;
		}
	}

	URenderSubsystem::URenderSubsystem(
		GraphicsDevice*        graphicsDevice,
		RenderResourceManager* renderResourceManager,
		ShaderLibrary*         shaderLibrary,
		RootSignatureCache*    rootSignatureCache,
		UPipelineCache*        pipelineCache
	) : mGraphicsDevice(graphicsDevice),
	    mRenderResourceManager(renderResourceManager),
	    mShaderLibrary(shaderLibrary),
	    mRootSignatureCache(rootSignatureCache),
	    mPipelineCache(pipelineCache) {
		ServiceLocator::Register<URenderSubsystem>(this);
	}

	bool URenderSubsystem::Init() {
		return true;
	}

	void URenderSubsystem::BeginFrame() {
		mItems.clear();

		for (auto& [keep, fence, fenceValue] : mTransient) {
			if (fence && fence->GetCompletedValue() >= fenceValue) {
				keep.clear();
				fence.Reset();
				fenceValue = 0;
			}
		}

		mContext = mGraphicsDevice->BeginFrame();

		const bool ok = mRenderResourceManager->GetUploadArena()->BeginFrame(
			mContext.backIndex);
		if (!ok) {
			auto fb = mGraphicsDevice->GetFrameBuffer(mContext.backIndex);
			fb.fence->SetEventOnCompletion(fb.fenceValue, fb.event);
			WaitForSingleObject(fb.event, INFINITE);
			mRenderResourceManager->GetUploadArena()->BeginFrame(
				mContext.backIndex);
		}

		auto& [keep, fence, fenceValue] = mTransient[mContext.backIndex];
		keep.clear();
		fence.Reset();
		fenceValue = 0;

		FrameCBData f;
		f = {
			.view = mView.view,
			.proj = mView.proj,
			.viewProj = mView.viewProj,
			.cameraPos = mView.cameraPos,
			.time = 0.0f
		};
		mFrameCBVA = UploadCB(&f, sizeof(f));
	}

	void URenderSubsystem::RenderWorld(const UWorld& world) {
		Collect(world, Mat4::identity);
		std::ranges::sort(
			mItems,
			[](const RenderItem& a, const RenderItem& b) {
				if (a.psoId != b.psoId) {
					return a.psoId < b.psoId;
				}
				if (a.materialKey != b.materialKey) {
					return a.materialKey < b.materialKey;
				}
				return a.depthVS < b.depthVS; // Front to Back (不透明用)
			}
		);

		// デバッグ: 描画順序を確認（最初の10個だけ）
		static bool logged = false;
		if (!logged && !mItems.empty()) {
			DevMsg("Render", "=== Draw Order (first 10) ===");
			for (size_t i = 0; i < std::min<size_t>(10, mItems.size()); ++i) {
				DevMsg(
					"Render",
					"[{}] depth={:.2f}, pso={}, mat={}",
					i,
					mItems[i].depthVS,
					mItems[i].psoId,
					mItems[i].materialKey
				);
			}
			logged = true;
		}

		DrawItems();
	}

	void URenderSubsystem::EndFrame() {
		mGraphicsDevice->EndFrame(mContext);

		auto buffer = mGraphicsDevice->GetFrameBuffer(mContext.backIndex);
		mTransient[mContext.backIndex].fence = buffer.fence;
		mTransient[mContext.backIndex].fenceValue = buffer.fenceValue;
		mRenderResourceManager->FlushUploads(
			buffer.fence.Get(), buffer.fenceValue
		);
	}

	void URenderSubsystem::Collect(const UWorld& world, const Mat4& parent) {
		for (auto& e : world.Entities()) {
			if (!e) { continue; }
			const auto* tr = e->GetComponent<TransformComponent>();
			auto*       mr = e->GetComponent<MeshRendererComponent>();
			if (tr && mr) {
				if (
					mr->EnsureGPU(
						mGraphicsDevice, mRenderResourceManager,
						mShaderLibrary, mRootSignatureCache,
						mPipelineCache, mContext.cmd
					)
				) {
					const Mat4 worldMat = tr->WorldMat() * parent;

					RenderItem it{};
					it.world      = worldMat;
					it.material   = &mr->material;
					it.meshHandle = mr->meshHandle; // 共有メッシュのハンドル

					it.depthVS = ComputeViewDepth(it.world, mView.view);

					it.psoId = mr->material.pso.id;
					it.materialKey = mr->materialAsset;
					it.rsPtr = mRootSignatureCache->Get(it.material->root);

					mItems.emplace_back(std::move(it));
				}
			}
		}

		for (const auto& child : world.Children()) {
			const auto& worldPtr        = child.world;
			const auto& parentTransform = child.parentTransform;
			Mat4        p               = parent;
			if (parentTransform) { p = parentTransform->WorldMat() * p; }
			if (worldPtr) { Collect(*worldPtr, p); }
		}
	}

	void URenderSubsystem::DrawItems() {
		auto* cmd = mContext.cmd;

		const std::array heaps = {
			mGraphicsDevice->GetSrvAllocator()->GetHeap(),
			mGraphicsDevice->GetSamplerAllocator()->GetHeap()
		};
		cmd->SetDescriptorHeaps(
			static_cast<UINT>(heaps.size()), heaps.data()
		);

		const ID3D12RootSignature* lastRs  = nullptr;
		uint32_t                   lastPso = 0;
		uint32_t                   lastMat = UINT32_MAX;

		for (auto& it : mItems) {
			if (it.psoId != lastPso || it.rsPtr != lastRs) {
				cmd->SetGraphicsRootSignature(it.rsPtr);
				cmd->SetPipelineState(mPipelineCache->Get({it.psoId}));
				lastPso = it.psoId;
				lastRs  = it.rsPtr;
				// ルートシグネチャが変わったら同じマテリアルでもう一度適用
				it.material->Apply(cmd, mRenderResourceManager,
				                   mContext.backIndex, 0.0f);
				lastMat = it.materialKey;
			}

			// マテリアル変更時に適用
			else if (it.materialKey != lastMat) {
				it.material->Apply(
					cmd, mRenderResourceManager, mContext.backIndex, 0.0f
				);
				lastMat = it.materialKey;
			}

			cmd->SetGraphicsRootConstantBufferView(
				it.material->rootParams.frameCB, mFrameCBVA
			);

			// オブジェクトコンスタントバッファ
			ObjectCBData o = {
				.world = it.world,
				.worldInverseTranspose = it.world.Inverse().Transpose()
			};
			D3D12_GPU_VIRTUAL_ADDRESS objCbGpu = UploadCB(&o, sizeof(o));
			UASSERT(objCbGpu != 0 && (objCbGpu & 0xFF) == 0);
			cmd->SetGraphicsRootConstantBufferView(
				it.material->rootParams.objectCB, objCbGpu);

			// 共有メッシュから実際のメッシュデータを取得
			const MeshGPU* mesh = mRenderResourceManager->
				GetMesh(it.meshHandle);
			if (!mesh) {
				continue; // メッシュが無効な場合はスキップ
			}

			mRenderResourceManager->BindVertexBuffer(cmd, mesh->vb);
			mRenderResourceManager->BindIndexBuffer(cmd, mesh->ib);
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			if (mesh->indexCount > 0) {
				cmd->DrawIndexedInstanced(
					mesh->indexCount,
					1,
					mesh->firstIndex,
					mesh->baseVertex,
					0
				);
			} else {
				cmd->DrawInstanced(3, 1, 0, 0);
			}
		}
	}

	D3D12_GPU_VIRTUAL_ADDRESS URenderSubsystem::UploadCB(
		const void* src, const size_t bytes
	) const {
		const size_t aligned = (bytes + 255) & ~static_cast<size_t>(255);
		const auto   slice = mRenderResourceManager->GetUploadArena()->Allocate(
			aligned, 256);
		if (!slice.cpu) {
			// 足りないときのフォールバック（警告だけ出して早期 return でもOK）
			Warning("Render",
			        "UploadArena capacity exceeded (size={}, frame={})",
			        aligned, mContext.backIndex);
			return 0;
		}
		std::memcpy(slice.cpu, src, bytes);
		return slice.gpuVirtualAddress;
	}
}
