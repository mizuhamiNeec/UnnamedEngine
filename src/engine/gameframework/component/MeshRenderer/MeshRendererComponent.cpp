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

		// メッシュの取得（共有機構を使用）
		if (!meshHandle.IsValid() && meshAsset != kInvalidAssetID) {
			meshHandle = renderResourceManager->AcquireMesh(meshAsset);
			if (!meshHandle.IsValid()) {
				Error(
					kChannel,
					"Failed to acquire mesh: {}",
					renderResourceManager->GetAssetManager()->Meta(meshAsset).name.c_str()
				);
				return false;
			}
		}

		mGPUReady = meshHandle.IsValid();

		return mGPUReady;
	}

	void MeshRendererComponent::InvalidateGPU(
		RenderResourceManager* renderResourceManager
	) {
		material.InvalidateGPU(renderResourceManager, nullptr, 0);
		
		// メッシュの解放
		if (meshHandle.IsValid()) {
			renderResourceManager->ReleaseMesh(meshHandle, nullptr, 0);
			meshHandle = {};
		}
		
		mGPUReady = false;
	}
}
