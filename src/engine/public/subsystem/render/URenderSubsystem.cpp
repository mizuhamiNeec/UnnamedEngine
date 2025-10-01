#include <pch.h>

#include <engine/public/gameframework/component/MeshRenderer/MeshRendererComponent.h>
#include <engine/public/gameframework/world/UWorld.h>
#include <engine/public/subsystem/interface/ServiceLocator.h>
#include <engine/public/subsystem/render/URenderSubsystem.h>

#include "engine/public/gameframework/component/Transform/TransformComponent.h"

namespace Unnamed {
	URenderSubsystem::URenderSubsystem(
		GraphicsDevice*        graphicsDevice,
		RenderResourceManager* renderResourceManager,
		ShaderLibrary*         shaderLibrary,
		RootSignatureCache*    rootSignatureCache,
		UPipelineCache*         pipelineCache
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

		for (auto& bin : mTransient) {
			if (bin.fence && bin.fence->GetCompletedValue() >= bin.fenceValue) {
				bin.keep.clear();
				bin.fence.Reset();
				bin.fenceValue = 0;
			}
		}

		mContext = mGraphicsDevice->BeginFrame();

		auto& bin = mTransient[mContext.backIndex];
		bin.keep.clear();
		bin.fence.Reset();
		bin.fenceValue = 0;

		FrameCBData f;
		f = {
			.view = mView.view,
			.proj = mView.proj,
			.viewProj = mView.viewProj,
			.cameraPos = mView.cameraPos,
			.time = 0.0f
		};
		UploadCB(&f, sizeof(f), mFrameCB);
		mFrameCBVA = mFrameCB.gpu;
		bin.keep.emplace_back(mFrameCB.resource);
	}

	void URenderSubsystem::RenderWorld(const UWorld& world) {
		Collect(world, Mat4::identity);
		DrawItems();
	}

	void URenderSubsystem::EndFrame() {
		mGraphicsDevice->EndFrame(mContext);

		auto buffer = mGraphicsDevice->GetFrameBuffer(mContext.backIndex);
		mTransient[mContext.backIndex].fence = buffer.fence;
		mTransient[mContext.backIndex].fenceValue = buffer.fenceValue;
	}

	void URenderSubsystem::Collect(const UWorld& world, const Mat4& parent) {
		for (auto& e : world.Entities()) {
			if (!e) { continue; }
			auto* tr = e->GetComponent<TransformComponent>();
			auto* mr = e->GetComponent<MeshRendererComponent>();
			if (tr && mr) {
				if (
					mr->EnsureGPU(
						mGraphicsDevice, mRenderResourceManager,
						mShaderLibrary, mRootSignatureCache,
						mPipelineCache, mContext.cmd
					)
				) {
					const Mat4 worldMat = parent * tr->WorldMat();
					mItems.emplace_back(
						Item{
							.world = worldMat,
							.mesh = &mr->mesh,
							.material = &mr->material
						}
					);
				}
			}
		}

		for (auto& cw : world.Children()) {
			Mat4 p = parent;
			if (cw.parentTransform) { p = p * cw.parentTransform->WorldMat(); }
			if (cw.world) { Collect(*cw.world, p); }
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

		mLastRS = nullptr;

		for (auto& it : mItems) {
			auto* rs  = mRootSignatureCache->Get(it.material->root);
			auto* pso = mPipelineCache->Get(it.material->pso);

			// 1 ルート/PSO
			cmd->SetGraphicsRootSignature(rs);
			cmd->SetPipelineState(pso);

			// マテリアルの適用
			it.material->Apply(
				cmd, mRenderResourceManager, mContext.backIndex, 0.0f
			);

			mContext.cmd->SetGraphicsRootConstantBufferView(
				it.material->rootParams.frameCB, mFrameCB.gpu
			);

			// オブジェクトコンスタントバッファ
			ObjectCBData o = {
				.world = it.world,
				.worldInverseTranspose = it.world.Inverse().Transpose()
			};
			TempCB objCB;
			UploadCB(&o, sizeof(o), objCB);
			UASSERT(objCB.gpu != 0 && (objCB.gpu & 0xFF) == 0);
			cmd->SetGraphicsRootConstantBufferView(
				it.material->rootParams.objectCB, objCB.gpu);
			mTransient[mContext.backIndex].keep.emplace_back(objCB.resource);

			mRenderResourceManager->BindVertexBuffer(cmd, it.mesh->vb);
			mRenderResourceManager->BindIndexBuffer(cmd, it.mesh->ib);
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			if (it.mesh->indexCount > 0) {
				cmd->DrawIndexedInstanced(
					it.mesh->indexCount,
					1,
					it.mesh->firstIndex,
					it.mesh->baseVertex,
					0
				);
			} else {
				cmd->DrawInstanced(3, 1, 0, 0);
			}
		}
	}

	D3D12_GPU_VIRTUAL_ADDRESS URenderSubsystem::UploadCB(
		const void* src, const size_t bytes, TempCB& out
	) const {
		const size_t aligned = (bytes + 255) & ~static_cast<size_t>(255);
		auto*        dev     = mGraphicsDevice->Device();

		D3D12_HEAP_PROPERTIES heapProps;
		heapProps = {
			.Type = D3D12_HEAP_TYPE_UPLOAD,
			.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
			.CreationNodeMask = 1,
			.VisibleNodeMask = 1
		};

		D3D12_RESOURCE_DESC desc;
		desc = D3D12_RESOURCE_DESC{
			.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
			.Alignment = 0,
			.Width = aligned,
			.Height = 1,
			.DepthOrArraySize = 1,
			.MipLevels = 1,
			.Format = DXGI_FORMAT_UNKNOWN,
			.SampleDesc = {1, 0},
			.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			.Flags = D3D12_RESOURCE_FLAG_NONE
		};

		out.resource.Reset();
		THROW(
			dev->CreateCommittedResource(
				&heapProps, D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&out.resource)
			)
		);

		void*                 ptr   = nullptr;
		constexpr D3D12_RANGE range = {0, 0};
		THROW(out.resource->Map(0, &range, &ptr));
		memcpy(ptr, src, bytes);
		out.resource->Unmap(0, nullptr);

		out.gpu = out.resource->GetGPUVirtualAddress();
		return out.gpu;
	}
}
