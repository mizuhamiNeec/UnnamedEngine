#pragma once
#include <d3d12.h>

#include <engine/public/gameframework/component/base/BaseComponent.h>

#include "runtime/assets/core/UAssetID.h"
#include "runtime/materials/UMaterialRuntime.h"
#include "runtime/render/types/RenderTypes.h"

namespace Unnamed {
	class RenderResourceManager;
	class ShaderLibrary;
	class RRootSignatureCache;
	class UPipelineCache;
	class GraphicsDevice;

	class MeshRendererComponent : public BaseComponent {
	public:
		AssetID meshAsset     = kInvalidAssetID;
		AssetID materialAsset = kInvalidAssetID;

		MeshGPU          mesh     = {};
		UMaterialRuntime material = {};

		// MeshRendererComponent
		[[nodiscard]] std::string_view GetComponentName() const override {
			return "MeshRenderer";
		}

		bool EnsureGPU(
			GraphicsDevice*        graphicsDevice,
			RenderResourceManager* renderResourceManager,
			ShaderLibrary*         shaderLibrary,
			RootSignatureCache*    rootSignatureCache,
			UPipelineCache*         pipelineCache, ID3D12GraphicsCommandList* cmd
		);

		void InvalidateGPU(RenderResourceManager* renderResourceManager);


		// BaseComponent interface
		void OnAttached() override;
		void OnDetached() override;

		void Serialize(JsonWriter& writer) const override;
		void Deserialize(const JsonReader& reader) override;

	private:
		bool mGPUReady = false;
	};
}
