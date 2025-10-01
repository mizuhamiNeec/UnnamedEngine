#include <pch.h>

//-----------------------------------------------------------------------------

#include <engine/public/gameframework/component/MeshRenderer/MeshRendererComponent.h>
#include <engine/public/urenderer/GraphicsDevice.h>

#include "core/json/JsonReader.h"

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
				Warning(kChannel, "Invalid mesh asset: {}", meshAsset);
				return false;
			}

			struct V {
				Vec3 p;
				Vec4 c;
				Vec2 uv;
			};
			std::vector<V> verts(m->positions.size());
			for (size_t i = 0; i < verts.size(); ++i) {
				verts[i].p  = m->positions[i];
				verts[i].c  = m->color0[i];
				verts[i].uv = m->uv0[i];
			}

			renderResourceManager->CreateStaticVertexBuffer(
				verts.data(), sizeof(V) * verts.size(), sizeof(V), mesh.vb
			);
			renderResourceManager->CreateStaticIndexBuffer(
				m->indices.data(), sizeof(uint32_t) * m->indices.size(),
				DXGI_FORMAT_R32_UINT, mesh.ib
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
