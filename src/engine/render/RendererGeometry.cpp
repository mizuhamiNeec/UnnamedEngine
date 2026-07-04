#include "Renderer.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "RenderDevice.h"
#include "RendererPipelineCatalog.h"

#include "core/assets/AssetManager.h"
#include "core/assets/types/MaterialAssetData.h"
#include "core/assets/types/MaterialInstanceAssetData.h"
#include "core/assets/types/MeshAssetData.h"
#include "core/assets/types/PostFxChainAssetData.h"
#include "core/assets/types/TextureAssetData.h"
#include "core/filesystem/VirtualPath.h"

#include "engine/rhi/Buffer.h"
#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/rhi/d3d12/D3D12Util.h"

namespace Unnamed::Render {
	namespace {
		float ToMaterialShadingModelValue(
			const MATERIAL_SHADING_MODEL shadingModel
		) {
			switch (shadingModel) {
				case MATERIAL_SHADING_MODEL::TOON: return 1.0f;
				case MATERIAL_SHADING_MODEL::UNLIT: return 2.0f;
				case MATERIAL_SHADING_MODEL::LIT_PBR:
				default: return 0.0f;
			}
		}

		const MatTextureOverride* ResolveMaterialTextureOverride(
			const MaterialInstanceAssetData& matInst,
			const MATERIAL_TEXTURE_SLOT      slot
		) {
			auto FindOverride = [&matInst](
				const char* key
			) -> const MatTextureOverride* {
				if (const auto it = matInst.textureOverrides.find(key);
					it != matInst.textureOverrides.end()) {
					return &it->second;
				}
				return nullptr;
			};

			switch (slot) {
				case MATERIAL_TEXTURE_SLOT::BASE_COLOR: {
					if (
						const MatTextureOverride* textureOverride =
							FindOverride("BaseColor")
					) {
						return textureOverride;
					}
					return FindOverride("MainTex");
				}
				case MATERIAL_TEXTURE_SLOT::NORMAL: {
					return FindOverride("Normal");
				}
				case MATERIAL_TEXTURE_SLOT::ORM: {
					return FindOverride("ORM");
				}
				case MATERIAL_TEXTURE_SLOT::EMISSIVE: {
					return FindOverride("Emissive");
				}
				case MATERIAL_TEXTURE_SLOT::COUNT:
				default: return nullptr;
			}
		}

		uint32_t GetFallbackTextureId(
			const MaterialTextureSet&   defaultTextures,
			const MATERIAL_TEXTURE_SLOT slot
		) {
			switch (slot) {
				case MATERIAL_TEXTURE_SLOT::BASE_COLOR: return defaultTextures.
						baseColorTextureId;
				case MATERIAL_TEXTURE_SLOT::NORMAL: return defaultTextures.
						normalTextureId;
				case MATERIAL_TEXTURE_SLOT::ORM: return defaultTextures.
						ormTextureId;
				case MATERIAL_TEXTURE_SLOT::EMISSIVE: return defaultTextures.
						emissiveTextureId;
				case MATERIAL_TEXTURE_SLOT::COUNT:
				default: return 0;
			}
		}

		void CreateDefaultBufferWithUpload(
			ID3D12Device*                           device,
			ID3D12GraphicsCommandList*              cmdList,
			const void*                             srcData,
			const uint64_t                          byteSize,
			Microsoft::WRL::ComPtr<ID3D12Resource>& outDefault,
			Microsoft::WRL::ComPtr<ID3D12Resource>& outUpload,
			const D3D12_RESOURCE_STATES             afterState
		) {
			{
				D3D12_HEAP_PROPERTIES heap = {};
				heap.Type                  = D3D12_HEAP_TYPE_DEFAULT;

				D3D12_RESOURCE_DESC desc = {};
				desc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
				desc.Width               = byteSize;
				desc.Height              = 1;
				desc.DepthOrArraySize    = 1;
				desc.MipLevels           = 1;
				desc.SampleDesc.Count    = 1;
				desc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

				Rhi::Throw(
					device->CreateCommittedResource(
						&heap, D3D12_HEAP_FLAG_NONE, &desc,
						D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
						IID_PPV_ARGS(outDefault.ReleaseAndGetAddressOf())
					)
				);
			}

			{
				D3D12_HEAP_PROPERTIES heap = {};
				heap.Type                  = D3D12_HEAP_TYPE_UPLOAD;

				D3D12_RESOURCE_DESC desc = {};
				desc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
				desc.Width               = byteSize;
				desc.Height              = 1;
				desc.DepthOrArraySize    = 1;
				desc.MipLevels           = 1;
				desc.SampleDesc.Count    = 1;
				desc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

				Rhi::Throw(
					device->CreateCommittedResource(
						&heap, D3D12_HEAP_FLAG_NONE, &desc,
						D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
						IID_PPV_ARGS(outUpload.ReleaseAndGetAddressOf())
					)
				);

				void*                 mapped = nullptr;
				constexpr D3D12_RANGE range  = {.Begin = 0, .End = 0};
				Rhi::Throw(outUpload->Map(0, &range, &mapped));
				memcpy(mapped, srcData, byteSize);
				outUpload->Unmap(0, nullptr);
			}

			cmdList->CopyBufferRegion(
				outDefault.Get(), 0, outUpload.Get(), 0, byteSize
			);

			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.pResource = outDefault.Get();
			barrier.Transition.Subresource =
				D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			barrier.Transition.StateAfter  = afterState;
			cmdList->ResourceBarrier(1, &barrier);
		}

		struct VertexGeom {
			float px,  py, pz;
			float nx,  ny, nz;
			float u,   v;
			float tx,  ty,  tz,  tw;
			float bi0, bi1, bi2, bi3;
			float bw0, bw1, bw2, bw3;
		};

		struct QuadVertex {
			float px, py, pz;
			float u,  v;
		};

		AABB MakeAabbFromPositions(const std::vector<VertexGeom>& vertices) {
			AABB aabb{};
			for (const auto& v : vertices) {
				aabb.Expand(Vec3(v.px, v.py, v.pz));
			}
			return aabb;
		}
	}

	void Renderer::CreateTriangleTestResources(Rhi::D3D12Device& dx) {
		auto& up = dx.GetUploadContext();
		up.Begin();

		auto* device  = dx.GetDevice();
		auto* cmdList = up.GetCommandList();

		constexpr VertexGeom verts[3] = {
			{
				.px  = -0.5f, .py = -0.5f, .pz = 0.0f,
				.nx  = 0, .ny     = 0, .nz     = 1,
				.u   = 0, .v      = 1,
				.bi0 = 0, .bi1    = 0, .bi2 = 0, .bi3 = 0,
				.bw0 = 1, .bw1    = 0, .bw2 = 0, .bw3 = 0
			},
			{
				.px  = 0.0f, .py = 0.5f, .pz = 0.0f,
				.nx  = 0, .ny    = 0, .nz    = 1,
				.u   = 0.5f, .v  = 0,
				.bi0 = 0, .bi1   = 0, .bi2 = 0, .bi3 = 0,
				.bw0 = 1, .bw1   = 0, .bw2 = 0, .bw3 = 0
			},
			{
				.px  = 0.5f, .py = -0.5f, .pz = 0.0f,
				.nx  = 0, .ny    = 0, .nz     = 1,
				.u   = 1, .v     = 1,
				.bi0 = 0, .bi1   = 0, .bi2 = 0, .bi3 = 0,
				.bw0 = 1, .bw1   = 0, .bw2 = 0, .bw3 = 0
			},
		};
		constexpr uint16_t indices[3] = {0, 1, 2};

		Microsoft::WRL::ComPtr<ID3D12Resource> vbUpload;
		Microsoft::WRL::ComPtr<ID3D12Resource> ibUpload;

		CreateDefaultBufferWithUpload(
			device,
			cmdList,
			verts,
			sizeof(verts),
			mGeometryPass.vb,
			vbUpload,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
		);

		CreateDefaultBufferWithUpload(
			device,
			cmdList,
			indices,
			sizeof(indices),
			mGeometryPass.ib,
			ibUpload,
			D3D12_RESOURCE_STATE_INDEX_BUFFER
		);

		up.EndAndSubmitAndWait();

		mGeometryPass.vbv.BufferLocation = mGeometryPass.vb->
			GetGPUVirtualAddress();
		mGeometryPass.vbv.SizeInBytes   = sizeof(verts);
		mGeometryPass.vbv.StrideInBytes = sizeof(VertexGeom);

		mGeometryPass.ibv.BufferLocation = mGeometryPass.ib->
			GetGPUVirtualAddress();
		mGeometryPass.ibv.SizeInBytes = sizeof(indices);
		mGeometryPass.ibv.Format      = DXGI_FORMAT_R16_UINT;
		mGeometryPass.indexCount      = 3;
		mGeometryPass.localAABB       = MakeAabbFromPositions(
			std::vector<VertexGeom>(std::begin(verts), std::end(verts))
		);

		mGeometryPass.vb->SetName(L"TriangleTestVB_Default");
		mGeometryPass.ib->SetName(L"TriangleTestIB_Default");
	}

	void Renderer::CreateQuadResources(Rhi::D3D12Device& dx) {
		auto& up = dx.GetUploadContext();
		up.Begin();

		auto* device  = dx.GetDevice();
		auto* cmdList = up.GetCommandList();

		constexpr QuadVertex verts[4] = {
			{.px = -1.0f, .py = -1.0f, .pz = 0.0f, .u = 0.0f, .v = 1.0f},
			{.px = -1.0f, .py = 1.0f, .pz = 0.0f, .u = 0.0f, .v = 0.0f},
			{.px = 1.0f, .py = 1.0f, .pz = 0.0f, .u = 1.0f, .v = 0.0f},
			{.px = 1.0f, .py = -1.0f, .pz = 0.0f, .u = 1.0f, .v = 1.0f},
		};
		constexpr uint16_t indices[6] = {0, 1, 2, 0, 2, 3};

		Microsoft::WRL::ComPtr<ID3D12Resource> vbUpload;
		Microsoft::WRL::ComPtr<ID3D12Resource> ibUpload;

		CreateDefaultBufferWithUpload(
			device,
			cmdList,
			verts,
			sizeof(verts),
			mSpritePass.geom.vb,
			vbUpload,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
		);

		CreateDefaultBufferWithUpload(
			device,
			cmdList,
			indices,
			sizeof(indices),
			mSpritePass.geom.ib,
			ibUpload,
			D3D12_RESOURCE_STATE_INDEX_BUFFER
		);

		up.EndAndSubmitAndWait();

		mSpritePass.geom.vbv.BufferLocation =
			mSpritePass.geom.vb->GetGPUVirtualAddress();
		mSpritePass.geom.vbv.SizeInBytes   = sizeof(verts);
		mSpritePass.geom.vbv.StrideInBytes = sizeof(QuadVertex);

		mSpritePass.geom.ibv.BufferLocation =
			mSpritePass.geom.ib->GetGPUVirtualAddress();
		mSpritePass.geom.ibv.SizeInBytes = sizeof(indices);
		mSpritePass.geom.ibv.Format      = DXGI_FORMAT_R16_UINT;
		mSpritePass.geom.indexCount      = 6;

		mSpritePass.geom.vb->SetName(L"QuadVB_Default");
		mSpritePass.geom.ib->SetName(L"QuadIB_Default");
	}

	void Renderer::CreateSkyboxCubeResources(Rhi::D3D12Device& dx) {
		auto& up = dx.GetUploadContext();
		up.Begin();

		auto* device  = dx.GetDevice();
		auto* cmdList = up.GetCommandList();

		constexpr VertexGeom verts[8] = {
			{
				.px  = -1.0f, .py = -1.0f, .pz = -1.0f,
				.nx  = 0, .ny     = 0, .nz     = 0,
				.u   = 0, .v      = 0,
				.bi0 = 0, .bi1    = 0, .bi2 = 0, .bi3 = 0,
				.bw0 = 1, .bw1    = 0, .bw2 = 0, .bw3 = 0
			},
			{
				.px  = 1.0f, .py = -1.0f, .pz = -1.0f,
				.nx  = 0, .ny    = 0, .nz     = 0,
				.u   = 0, .v     = 0,
				.bi0 = 0, .bi1   = 0, .bi2 = 0, .bi3 = 0,
				.bw0 = 1, .bw1   = 0, .bw2 = 0, .bw3 = 0
			},
			{
				.px  = 1.0f, .py = 1.0f, .pz = -1.0f,
				.nx  = 0, .ny    = 0, .nz    = 0,
				.u   = 0, .v     = 0,
				.bi0 = 0, .bi1   = 0, .bi2 = 0, .bi3 = 0,
				.bw0 = 1, .bw1   = 0, .bw2 = 0, .bw3 = 0
			},
			{
				.px  = -1.0f, .py = 1.0f, .pz = -1.0f,
				.nx  = 0, .ny     = 0, .nz    = 0,
				.u   = 0, .v      = 0,
				.bi0 = 0, .bi1    = 0, .bi2 = 0, .bi3 = 0,
				.bw0 = 1, .bw1    = 0, .bw2 = 0, .bw3 = 0
			},
			{
				.px  = -1.0f, .py = -1.0f, .pz = 1.0f,
				.nx  = 0, .ny     = 0, .nz     = 0,
				.u   = 0, .v      = 0,
				.bi0 = 0, .bi1    = 0, .bi2 = 0, .bi3 = 0,
				.bw0 = 1, .bw1    = 0, .bw2 = 0, .bw3 = 0
			},
			{
				.px  = 1.0f, .py = -1.0f, .pz = 1.0f,
				.nx  = 0, .ny    = 0, .nz     = 0,
				.u   = 0, .v     = 0,
				.bi0 = 0, .bi1   = 0, .bi2 = 0, .bi3 = 0,
				.bw0 = 1, .bw1   = 0, .bw2 = 0, .bw3 = 0
			},
			{
				.px  = 1.0f, .py = 1.0f, .pz = 1.0f,
				.nx  = 0, .ny    = 0, .nz    = 0,
				.u   = 0, .v     = 0,
				.bi0 = 0, .bi1   = 0, .bi2 = 0, .bi3 = 0,
				.bw0 = 1, .bw1   = 0, .bw2 = 0, .bw3 = 0
			},
			{
				.px  = -1.0f, .py = 1.0f, .pz = 1.0f,
				.nx  = 0, .ny     = 0, .nz    = 0,
				.u   = 0, .v      = 0,
				.bi0 = 0, .bi1    = 0, .bi2 = 0, .bi3 = 0,
				.bw0 = 1, .bw1    = 0, .bw2 = 0, .bw3 = 0
			},
		};
		constexpr uint16_t indices[36] = {
			0, 1, 2, 0, 2, 3,
			4, 6, 5, 4, 7, 6,
			4, 5, 1, 4, 1, 0,
			3, 2, 6, 3, 6, 7,
			1, 5, 6, 1, 6, 2,
			4, 0, 3, 4, 3, 7,
		};

		Microsoft::WRL::ComPtr<ID3D12Resource> vbUpload;
		Microsoft::WRL::ComPtr<ID3D12Resource> ibUpload;

		CreateDefaultBufferWithUpload(
			device,
			cmdList,
			verts,
			sizeof(verts),
			mSkyboxPass.geom.vb,
			vbUpload,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
		);

		CreateDefaultBufferWithUpload(
			device,
			cmdList,
			indices,
			sizeof(indices),
			mSkyboxPass.geom.ib,
			ibUpload,
			D3D12_RESOURCE_STATE_INDEX_BUFFER
		);

		up.EndAndSubmitAndWait();

		mSkyboxPass.geom.vbv.BufferLocation =
			mSkyboxPass.geom.vb->GetGPUVirtualAddress();
		mSkyboxPass.geom.vbv.SizeInBytes   = sizeof(verts);
		mSkyboxPass.geom.vbv.StrideInBytes = sizeof(VertexGeom);

		mSkyboxPass.geom.ibv.BufferLocation =
			mSkyboxPass.geom.ib->GetGPUVirtualAddress();
		mSkyboxPass.geom.ibv.SizeInBytes = sizeof(indices);
		mSkyboxPass.geom.ibv.Format      = DXGI_FORMAT_R16_UINT;
		mSkyboxPass.geom.indexCount      = 36;
		mSkyboxPass.geom.localAABB       = MakeAabbFromPositions(
			std::vector<VertexGeom>(std::begin(verts), std::end(verts))
		);

		mSkyboxPass.geom.vb->SetName(L"SkyboxCubeVB_Default");
		mSkyboxPass.geom.ib->SetName(L"SkyboxCubeIB_Default");
	}

	bool Renderer::EnsureMeshResourceLoaded(
		RenderDevice& renderDevice, Rhi::D3D12Device& dx,
		const AssetID meshAssetId
	) {
		if (meshAssetId == kInvalidAssetID) {
			return false;
		}
		if (mSceneMeshesByAsset.contains(meshAssetId)) {
			return true;
		}

		const auto& assetManager = renderDevice.GetAssetManager();
		const auto* meshAsset    = assetManager.Get<MeshAssetData>(meshAssetId);
		if (!meshAsset || meshAsset->vertices.empty() || meshAsset->indices.
		    empty()) {
			return false;
		}

		std::vector<VertexGeom> vertices;
		vertices.reserve(meshAsset->vertices.size());
		for (const auto& v : meshAsset->vertices) {
			float weightSum = 0.0f;
			for (const float w : v.boneWeights) {
				weightSum += w;
			}
			const float w0 = weightSum > 0.0f ? v.boneWeights[0] : 1.0f;
			const float w1 = weightSum > 0.0f ? v.boneWeights[1] : 0.0f;
			const float w2 = weightSum > 0.0f ? v.boneWeights[2] : 0.0f;
			const float w3 = weightSum > 0.0f ? v.boneWeights[3] : 0.0f;
			vertices.emplace_back(
				VertexGeom{
					.px  = v.position.x, .py = v.position.y, .pz = v.position.z,
					.nx  = v.normal.x, .ny   = v.normal.y, .nz   = v.normal.z,
					.u   = v.uv.x, .v        = v.uv.y,
					.tx  = v.tangent.x, .ty  = v.tangent.y,
					.tz  = v.tangent.z, .tw  = v.tangent.w,
					.bi0 = static_cast<float>(v.boneIndices[0]),
					.bi1 = static_cast<float>(v.boneIndices[1]),
					.bi2 = static_cast<float>(v.boneIndices[2]),
					.bi3 = static_cast<float>(v.boneIndices[3]),
					.bw0 = w0, .bw1 = w1, .bw2 = w2, .bw3 = w3
				}
			);
		}

		auto& up = dx.GetUploadContext();
		up.Begin();

		auto* device  = dx.GetDevice();
		auto* cmdList = up.GetCommandList();

		MeshBuffer                             meshBuffer = {};
		Microsoft::WRL::ComPtr<ID3D12Resource> vbUpload;
		Microsoft::WRL::ComPtr<ID3D12Resource> ibUpload;

		CreateDefaultBufferWithUpload(
			device,
			cmdList,
			vertices.data(),
			sizeof(VertexGeom) * vertices.size(),
			meshBuffer.vb,
			vbUpload,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
		);

		const uint32_t maxIndex = *std::ranges::max_element(meshAsset->indices);
		if (maxIndex <= 0xFFFFu) {
			std::vector<uint16_t> indices16;
			indices16.reserve(meshAsset->indices.size());
			for (const uint32_t i : meshAsset->indices) {
				indices16.emplace_back(static_cast<uint16_t>(i));
			}

			CreateDefaultBufferWithUpload(
				device,
				cmdList,
				indices16.data(),
				sizeof(uint16_t) * indices16.size(),
				meshBuffer.ib,
				ibUpload,
				D3D12_RESOURCE_STATE_INDEX_BUFFER
			);

			meshBuffer.ibv.Format      = DXGI_FORMAT_R16_UINT;
			meshBuffer.ibv.SizeInBytes = static_cast<UINT>(
				sizeof(uint16_t) * indices16.size()
			);
		} else {
			CreateDefaultBufferWithUpload(
				device,
				cmdList,
				meshAsset->indices.data(),
				sizeof(uint32_t) * meshAsset->indices.size(),
				meshBuffer.ib,
				ibUpload,
				D3D12_RESOURCE_STATE_INDEX_BUFFER
			);

			meshBuffer.ibv.Format      = DXGI_FORMAT_R32_UINT;
			meshBuffer.ibv.SizeInBytes = static_cast<UINT>(
				sizeof(uint32_t) * meshAsset->indices.size()
			);
		}

		up.EndAndSubmitAndWait();

		meshBuffer.vbv.BufferLocation = meshBuffer.vb->GetGPUVirtualAddress();
		meshBuffer.vbv.SizeInBytes    = static_cast<UINT>(
			sizeof(VertexGeom) * vertices.size()
		);
		meshBuffer.vbv.StrideInBytes = sizeof(VertexGeom);

		meshBuffer.ibv.BufferLocation = meshBuffer.ib->GetGPUVirtualAddress();
		meshBuffer.indexCount = static_cast<uint32_t>(meshAsset->indices.
			size());
		if (!meshAsset->submeshes.empty()) {
			meshBuffer.submeshes.reserve(meshAsset->submeshes.size());
			for (const auto& submesh : meshAsset->submeshes) {
				if (submesh.indexCount == 0) {
					continue;
				}

				MeshSubMeshRange range = {};
				range.indexStart       = submesh.indexStart;
				range.indexCount       = submesh.indexCount;
				range.materialIndex    = submesh.materialIndex;
				meshBuffer.submeshes.emplace_back(range);
			}
		}
		if (meshBuffer.submeshes.empty() && meshBuffer.indexCount > 0) {
			meshBuffer.submeshes.emplace_back(
				MeshSubMeshRange{
					.indexStart    = 0,
					.indexCount    = meshBuffer.indexCount,
					.materialIndex = 0
				}
			);
		}
		meshBuffer.localAABB.min = meshAsset->localBoundsMin;
		meshBuffer.localAABB.max = meshAsset->localBoundsMax;

		meshBuffer.vb->SetName(L"SceneMesh_VB");
		meshBuffer.ib->SetName(L"SceneMesh_IB");

		const auto [it, inserted] = mSceneMeshesByAsset.emplace(
			meshAssetId, std::move(meshBuffer)
		);
		if (inserted) {
			mLoadedMeshAsset = meshAssetId;
		}
		if (mSceneMeshes.empty()) {
			mSceneMeshes.emplace_back(it->second);
		}
		return true;
	}

	void Renderer::LoadSceneMeshResources(
		RenderDevice& renderDevice, Rhi::D3D12Device& dx
	) {
		(void)renderDevice;
		(void)dx;
		// 旧 hand.gltf のハードコード初期読み込みは廃止。
		// 描画対象メッシュは World::FillRenderFrameInputs から供給される。
		mSceneMeshes.clear();
		mLoadedMeshAsset = kInvalidAssetID;
	}

	const VirtualPath kDefaultMaterialInstance = VirtualPath::ParseOrThrow(
		"materials/instances/dev_default.matinst.json"
	);

	void Renderer::LoadMaterialResources(
		RenderDevice& renderDevice, Rhi::D3D12Device& dx
	) {
		auto&                assetManager = renderDevice.GetAssetManager();
		std::vector<AssetID> requestedMaterialInstances = {};
		EnsureDefaultMaterialTextures(renderDevice);

		const AssetID materialInstanceId = assetManager.LoadMaterialInstance(
			kDefaultMaterialInstance
		);
		if (materialInstanceId != kInvalidAssetID) {
			requestedMaterialInstances.emplace_back(materialInstanceId);
			mDefaultMaterialInstance = materialInstanceId;
		}

		// 可視オブジェクトが参照する全マテリアルインスタンスを収集します。
		for (const RenderViewInput& view : mFrameViews) {
			if (view.type != RENDER_VIEW_TYPE::SCENE) {
				continue;
			}

			for (const VisibleRenderObject& object : view.visibleObjects) {
				if (object.materialInstanceId != kInvalidAssetID) {
					requestedMaterialInstances.emplace_back(
						object.materialInstanceId
					);
				}
				for (const AssetID slotMaterialId : object.
				     materialInstanceIdsBySlot
				) {
					if (slotMaterialId != kInvalidAssetID) {
						requestedMaterialInstances.emplace_back(slotMaterialId);
					}
				}
			}
		}

		if (requestedMaterialInstances.empty()) {
			return;
		}
		std::ranges::sort(requestedMaterialInstances
		);
		requestedMaterialInstances.erase(
			std::ranges::unique(
				requestedMaterialInstances
			).begin(),
			requestedMaterialInstances.end()
		);

		for (const AssetID requestedMaterialInstanceId :
		     requestedMaterialInstances
		) {
			if (requestedMaterialInstanceId == kInvalidAssetID) {
				continue;
			}
			if (mMaterialBindings.contains(requestedMaterialInstanceId)) {
				continue;
			}

			const auto* matInst = assetManager.Get<MaterialInstanceAssetData>(
				requestedMaterialInstanceId
			);
			if (!matInst || matInst->materialId == kInvalidAssetID) {
				continue;
			}

			const auto* mat = assetManager.Get<MaterialAssetData>(
				matInst->materialId
			);
			if (!mat) {
				continue;
			}

			MaterialBinding binding    = {};
			binding.materialInstanceId = requestedMaterialInstanceId;
			binding.shaderProgramId    = mat->shaderProgramId;
			binding.renderState        = mat->renderState;

			// Material pipeline warnings are emitted when the binding is created, so they do not repeat every frame.
			if (binding.shaderProgramId == kInvalidAssetID) {
				Warning(
					"Renderer",
					"Material instance {} has invalid shaderProgramId. Falling back to default geometry shader.",
					requestedMaterialInstanceId
				);
				binding.shaderProgramId = mGeometryShaderProgramId;
			}

			if (binding.renderState.blendEnable) {
				Warning(
					"Renderer",
					"Material instance {} has blendEnable=true, but transparent geometry is not implemented. Using opaque geometry fallback.",
					requestedMaterialInstanceId
				);
				binding.renderState.blendEnable = false;
			}

			if (
				binding.shaderProgramId != kInvalidAssetID &&
				binding.shaderProgramId != mGeometryShaderProgramId
			) {
				DevMsg(
					"Renderer",
					"Material instance {} uses custom geometry shader {}. Assuming GeomRootSignature compatibility including MaterialTextures(t0..t3), ShadowConstants(b4), ShadowMap(t4), and EnvironmentLighting(b5); shader reflection is not available.",
					requestedMaterialInstanceId,
					binding.shaderProgramId
				);
			}

			if (
				binding.shaderProgramId != kInvalidAssetID &&
				mGeometryVertexLayout.stride != 0
			) {
				// See Renderer::MaterialBinding for the current Geometry material shader contract.
				GraphicsPipelineSpec spec =
					RendererPipelineCatalog::MakeGeometryPreset(
						"MaterialGeometry_" +
						std::to_string(requestedMaterialInstanceId),
						binding.shaderProgramId,
						dx.GetGeomRootSignature(),
						kSceneHdrColorFormat,
						DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
						mGeometryVertexLayout
					);
				spec.psoTemplate.depthEnable =
					binding.renderState.depthEnable;
				spec.psoTemplate.depthWriteEnable =
					binding.renderState.depthWrite;
				spec.psoTemplate.cullMode =
					binding.renderState.cullBackFace ?
						D3D12_CULL_MODE_BACK :
						D3D12_CULL_MODE_NONE;
				spec.psoTemplate.blendEnable   = false;
				spec.psoTemplate.stencilEnable =
					binding.renderState.stencilEnable;
				spec.psoTemplate.stencilReadMask =
					binding.renderState.stencilReadMask;
				spec.psoTemplate.stencilWriteMask =
					binding.renderState.stencilWriteMask;
				binding.geometryPipeline = mPipelineRegistry.RegisterGraphics(
					spec
				);
			} else {
				Warning(
					"Renderer",
					"Material instance {} could not register a geometry pipeline variant. Falling back to default geometry pipeline.",
					requestedMaterialInstanceId
				);
				binding.geometryPipeline              = mGeometryPass.pipeline;
				binding.pipelineResolveWarningEmitted = true;
			}

			if (const auto it = mat->vectorParams.find("BaseColor");
				it != mat->vectorParams.end()) {
				binding.constants.baseColor = it->second;
			}
			if (const auto it = mat->vectorParams.find("EmissiveColor");
				it != mat->vectorParams.end()) {
				binding.constants.emissiveColor = it->second;
			}
			if (const auto it = mat->scalarParams.find("Metallic");
				it != mat->scalarParams.end()) {
				binding.constants.metallic = it->second;
			}
			if (const auto it = mat->scalarParams.find("Roughness");
				it != mat->scalarParams.end()) {
				binding.constants.roughness = it->second;
			}
			if (const auto it = mat->scalarParams.find("Opacity");
				it != mat->scalarParams.end()) {
				binding.constants.opacity = it->second;
			}
			binding.constants.shadingModel = ToMaterialShadingModelValue(
				mat->shadingModel
			);
			binding.constants.domainMode =
				binding.constants.shadingModel > 1.5f ? 0.0f : 1.0f;

			if (const auto it = matInst->vectorOverrides.find("BaseColor");
				it != matInst->vectorOverrides.end()) {
				binding.constants.baseColor = it->second;
			}
			if (const auto it = matInst->vectorOverrides.find("EmissiveColor");
				it != matInst->vectorOverrides.end()) {
				binding.constants.emissiveColor = it->second;
			}
			if (const auto it = matInst->scalarOverrides.find("Metallic");
				it != matInst->scalarOverrides.end()) {
				binding.constants.metallic = it->second;
			}
			if (const auto it = matInst->scalarOverrides.find("Roughness");
				it != matInst->scalarOverrides.end()) {
				binding.constants.roughness = it->second;
			}
			if (const auto it = matInst->scalarOverrides.find("Opacity");
				it != matInst->scalarOverrides.end()) {
				binding.constants.opacity = it->second;
			}

			auto resolveTexture = [&](const MATERIAL_TEXTURE_SLOT slot) {
				const MatTextureOverride* textureOverride =
					ResolveMaterialTextureOverride(*matInst, slot);
				if (textureOverride == nullptr) {
					return GetFallbackTextureId(
						mDefaultMaterialTextures, slot
					);
				}

				const AssetID texId = textureOverride->assetId;
				if (slot == MATERIAL_TEXTURE_SLOT::NORMAL) {
					const auto* texture = assetManager.Get<TextureAssetData>(
						texId
					);
					if (texture && texture->isSRGB) {
						Warning(
							"Renderer",
							"Material instance {} uses an sRGB Normal texture '{}'. Tangent-space DirectX normal maps must be linear.",
							requestedMaterialInstanceId,
							textureOverride->assetPath.String()
						);
					}
				}
				if (slot == MATERIAL_TEXTURE_SLOT::ORM) {
					const auto* texture = assetManager.Get<TextureAssetData>(
						texId
					);
					if (texture && texture->isSRGB) {
						Warning(
							"Renderer",
							"Material instance {} uses an sRGB ORM texture '{}'. ORM must be linear: R=AO, G=Perceptual Roughness, B=Metallic.",
							requestedMaterialInstanceId,
							textureOverride->assetPath.String()
						);
					}
				}
				const uint32_t textureId =
					mTextureResourceCache.ResolveTexture2D(texId);
				if (textureId != 0) {
					return textureId;
				}
				return GetFallbackTextureId(mDefaultMaterialTextures, slot);
			};

			binding.textures.baseColorTextureId =
				resolveTexture(MATERIAL_TEXTURE_SLOT::BASE_COLOR);
			binding.textures.normalTextureId =
				resolveTexture(MATERIAL_TEXTURE_SLOT::NORMAL);
			binding.textures.ormTextureId =
				resolveTexture(MATERIAL_TEXTURE_SLOT::ORM);
			binding.textures.emissiveTextureId =
				resolveTexture(MATERIAL_TEXTURE_SLOT::EMISSIVE);

			mMaterialBindings.emplace(
				requestedMaterialInstanceId,
				std::move(binding)
			);
		}
	}

	constexpr std::string_view kDefaultPostFxChainPath =
		"postfx/default.postfx.json";

	bool Renderer::LoadPostFxChain(const RenderDevice& renderDevice) {
		auto&       assetManager = renderDevice.GetAssetManager();
		const auto& dx           = static_cast<Rhi::D3D12Device&>(renderDevice.
			GetRhiDevice());
		if (mPostFxChainAsset == kInvalidAssetID) {
			mPostFxChainAsset = LoadCoreAsset(
				assetManager, kDefaultPostFxChainPath, ASSET_TYPE::POST_FX_CHAIN
			);
		}

		const auto* chain = assetManager.Get<PostFxChainAssetData>(
			mPostFxChainAsset);
		if (!chain) {
			mPostFxPasses.clear();
			return false;
		}

		std::vector<PostFxRuntimePass> runtimePasses;
		runtimePasses.reserve(chain->passes.size());

		for (const auto& passAsset : chain->passes) {
			const AssetID shaderProgramId = passAsset.shaderProgramId;
			if (shaderProgramId == kInvalidAssetID) {
				mPostFxPasses.clear();
				return false;
			}
			if (!ValidateShaderProgramStages(
				assetManager,
				shaderProgramId,
				RequiredShaderStages::Graphics,
				passAsset.name
			)) {
				mPostFxPasses.clear();
				return false;
			}

			PostFxRuntimePass runtimePass = {};
			runtimePass.name              = passAsset.name;
			runtimePass.enabled           = passAsset.enabled;
			runtimePass.scalarDefaults    = passAsset.scalarParams;
			runtimePass.colorDefaults     = passAsset.colorParams;
			runtimePass.pass.pipeline     = mPipelineRegistry.RegisterGraphics(
				RendererPipelineCatalog::MakeFullscreenPreset(
					"PostFx_" + runtimePass.name,
					shaderProgramId,
					dx.GetFsRootSignature(),
					kSceneHdrColorFormat
				)
			);
			runtimePass.pass.resolved = nullptr;

			runtimePasses.emplace_back(std::move(runtimePass));
		}

		mPostFxPasses = std::move(runtimePasses);
		return true;
	}

	uint32_t Renderer::EnsureSpriteTextureLoaded(
		RenderDevice& renderDevice, const AssetID textureAssetId
	) {
		if (textureAssetId == kInvalidAssetID) {
			EnsureSpriteFallbackTexture(renderDevice);
			return mSpriteFallbackTextureId;
		}

		const auto& assetManager = renderDevice.GetAssetManager();
		const auto* tex = assetManager.Get<TextureAssetData>(textureAssetId);
		if (!tex) {
			Warning(
				"Renderer",
				"EnsureSpriteTextureLoaded failed: TextureAssetData not found for assetId={}",
				textureAssetId
			);
			EnsureSpriteFallbackTexture(renderDevice);
			return mSpriteFallbackTextureId;
		}

		static bool       sLoggedFontAtlasTexturePath = false;
		const std::string textureSourcePath = tex->sourcePath.ToGenericUtf8();
		if (
			!sLoggedFontAtlasTexturePath &&
			textureSourcePath.find("runtime://ui/font_atlas_ascii") !=
			std::string::npos
		) {
			const size_t mipCount  = tex->mips.size();
			const size_t rowPitch0 = mipCount > 0 ? tex->mips[0].rowPitch : 0;
			const size_t dataSize0 = mipCount > 0 ?
				                         tex->mips[0].bytes.size() :
				                         0;
			DevMsg(
				"Renderer",
				"Font atlas texture asset resolved: assetId={}, sourcePath='{}', size={}x{}, format={}, mips={}, rowPitch0={}, dataSize0={}.",
				textureAssetId,
				tex->sourcePath,
				tex->width,
				tex->height,
				static_cast<int>(tex->format),
				mipCount,
				rowPitch0,
				dataSize0
			);
			sLoggedFontAtlasTexturePath = true;
		}

		const uint32_t textureId = mTextureResourceCache.ResolveSpriteTexture(
			textureAssetId
		);
		if (textureId == 0) {
			Warning(
				"Renderer",
				"EnsureSpriteTextureLoaded failed: TextureResourceCache::ResolveSpriteTexture returned 0 for assetId={}",
				textureAssetId
			);
			EnsureSpriteFallbackTexture(renderDevice);
			return mSpriteFallbackTextureId;
		}
		return textureId;
	}

	uint32_t Renderer::EnsureSkyboxTextureLoaded(
		RenderDevice& renderDevice, const AssetID textureAssetId
	) {
		(void)renderDevice;
		if (textureAssetId == kInvalidAssetID) {
			return 0;
		}
		const uint32_t textureId = mTextureResourceCache.ResolveSkyboxTexture(
			textureAssetId
		);
		if (textureId == 0) {
			return 0;
		}
		return textureId;
	}

	void Renderer::EnsureSpriteFallbackTexture(RenderDevice& renderDevice) {
		if (mSpriteFallbackTextureId != 0) {
			return;
		}

		TextureAssetData white = {};
		white.width            = 1;
		white.height           = 1;
		white.isSRGB           = true;
		TextureMip mip         = {};
		mip.width              = 1;
		mip.height             = 1;
		mip.rowPitch           = 4;
		mip.bytes              = {255, 255, 255, 255};
		white.mips.emplace_back(std::move(mip));
		mSpriteFallbackTextureId = renderDevice.GetRegistry().
		                                        CreateTexture2DFromAsset(
			                                        white,
			                                        "SpriteOverlayFallbackWhite"
		                                        );
	}
}
