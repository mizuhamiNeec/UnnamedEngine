#pragma once
#include "rendergraph/RgResourceRegistry.h"

#include "shaders/PipelineCache.h"
#include "shaders/ShaderLibrary.h"

#include <unordered_set>

namespace Unnamed {
	class AssetManager;
}

namespace Unnamed::Rhi {
	class IRhiDevice;
}

namespace Unnamed::Render {
	class RenderDevice {
	public:
		RenderDevice(Rhi::IRhiDevice& rhiDevice, AssetManager& assetManager);
		~RenderDevice();

		[[nodiscard]] AssetManager&    GetAssetManager() const;
		[[nodiscard]] Rhi::IRhiDevice& GetRhiDevice() const;

		ShaderLibrary& GetShaderLibrary();
		PipelineCache& GetPipelineCache();
		void           InvalidateAssetDerivedState(AssetID id);
		void           ConsumeDirtyAssets(
			std::unordered_set<AssetID>& outMeshAssets,
			bool&                        outMaterialsDirty,
			bool&                        outPostFxDirty
		);

		void                OnResize(uint32_t width, uint32_t height);
		RgResourceRegistry& GetRegistry();

		/// @brief GPU 完了待ち後、退役済み Registry リソースを回収します。
		void FlushGpuAndCollectGarbage();

	private:
		void HookHotReload();

		Rhi::IRhiDevice& mRhiDevice;
		AssetManager&    mAssetManager;

		ShaderLibrary mShaderLibrary;
		PipelineCache mPipelineCache;

		RgResourceRegistry          mRegistry;
		std::unordered_set<AssetID> mDirtyMeshAssets;
		bool                        mMaterialsDirty = false;
		bool                        mPostFxDirty    = false;
	};
}
