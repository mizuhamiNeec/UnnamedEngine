#include <pch.h>

//-----------------------------------------------------------------------------

#include <engine/gameframework/component/MeshRenderer/MeshRendererComponent.h>
#include <engine/urenderer/GraphicsDevice.h>

#include "core/json/JsonReader.h"

#include "engine/VertexFormats.h"

#include "runtime/assets/core/UAssetManager.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "MeshRendererComponent";

	void MeshRendererComponent::OnAttached() {
		BaseComponent::OnAttached();
	}

	void MeshRendererComponent::OnDetached() {
		BaseComponent::OnDetached();
	}

	void MeshRendererComponent::Serialize(JsonWriter& writer) const {
		(void)writer;
	}

	void MeshRendererComponent::Deserialize(const JsonReader& reader) {
		if (reader.Has("mesh")) {
			// TODO: AssetManagerからロード
		}
		if (reader.Has("material")) {
			// 文字列をAssetに解決して materialAsset
		}
	}

	bool MeshRendererComponent::EnsureGPU(
		GraphicsDevice*            graphicsDevice,
		RenderResourceManager*     renderResourceManager,
		ShaderLibrary*             shaderLibrary,
		RootSignatureCache*        rootSignatureCache,
		UPipelineCache*            pipelineCache,
		ID3D12GraphicsCommandList* cmd
	) {
		if (mGPUReady) { return true; }

		if (
			material.materialAsset == kInvalidAssetID &&
			materialAsset != kInvalidAssetID
		) {
			material.materialAsset = materialAsset;
		}

		if (
			!material.BuildCPU(
				renderResourceManager->GetAssetManager(), shaderLibrary,
				rootSignatureCache, pipelineCache, graphicsDevice
			)
		) {
			return false;
		}

		if (!material.IsGPUReady()) {
			material.RealizeGPU(renderResourceManager, cmd);
		}

		if (mesh.vb.handle.id == 0 && meshAsset != kInvalidAssetID) {
			const auto* m = renderResourceManager->GetAssetManager()->Get<
				MeshAssetData>(meshAsset);
			if (!m || m->positions.empty() || m->indices.empty()) {
				Error(
					kChannel,
					"Mesh asset is invalid: {}",
					renderResourceManager->GetAssetManager()->Meta(meshAsset).
					                       name.c_str()
				);
				return false;
			}

			const auto vcount = m->positions.size();

			std::vector<VertexPNUV> verts(vcount);

			for (size_t i = 0; i < vcount; ++i) {
				verts[i].position = m->positions[i];
				verts[i].normal   = m->normals[i];
				verts[i].uv       = m->uv0[i];
			}

			renderResourceManager->CreateStaticVertexBuffer(
				verts.data(),
				verts.size() * sizeof(VertexPNUV),
				sizeof(VertexPNUV),
				mesh.vb
			);

			renderResourceManager->CreateStaticIndexBuffer(
				m->indices.data(),
				m->indices.size() * sizeof(uint32_t),
				DXGI_FORMAT_R32_UINT,
				mesh.ib
			);

			mesh.indexCount = static_cast<uint32_t>(m->indices.size());
			mesh.firstIndex = 0;
			mesh.baseVertex = 0;
		}

		mGPUReady = mesh.vb.handle.id != 0 && mesh.ib.handle.id != 0;

		return mGPUReady;
	}

	void MeshRendererComponent::InvalidateGPU(
		RenderResourceManager* renderResourceManager
	) {
		material.InvalidateGPU(renderResourceManager, nullptr, 0);
		mGPUReady = false;
	}
}
