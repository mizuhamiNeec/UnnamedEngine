#include "RenderDevice.h"

#include <chrono>
#include <string_view>

#include "core/assets/AssetManager.h"

#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Render {
	namespace {
		constexpr std::string_view kChannel = "RenderDevice";
	}

	RenderDevice::RenderDevice(
		Rhi::IRhiDevice& rhiDevice, AssetManager& assetManager
	) : mRhiDevice(rhiDevice),
	    mAssetManager(assetManager),
	    mShaderLibrary(
		    assetManager,
		    dynamic_cast<Rhi::D3D12Device&>(rhiDevice).GetDxcCompiler()
	    ),
	    mPipelineCache(
		    dynamic_cast<Rhi::D3D12Device&>(rhiDevice).GetDevice(),
		    mShaderLibrary
	    ),
	    mRegistry(dynamic_cast<Rhi::D3D12Device&>(rhiDevice)) {
		HookHotReload();
	}

	RenderDevice::~RenderDevice() {
		mShaderLibrary.InvalidateAll();
	}

	AssetManager& RenderDevice::GetAssetManager() const {
		return mAssetManager;
	}

	Rhi::IRhiDevice& RenderDevice::GetRhiDevice() const {
		return mRhiDevice;
	}

	ShaderLibrary& RenderDevice::GetShaderLibrary() {
		return mShaderLibrary;
	}

	PipelineCache& RenderDevice::GetPipelineCache() {
		return mPipelineCache;
	}

	void RenderDevice::InvalidateAssetDerivedState(const AssetID id) {
		const auto& meta = mAssetManager.Meta(id);
		switch (meta.type) {
			case ASSET_TYPE::SHADER_SOURCE: {
				mShaderLibrary.MarkDirtyByShaderSource(id);
				mPipelineCache.MarkDirtyByShaderSource(id);
				break;
			}
			case ASSET_TYPE::SHADER_PROGRAM: {
				mShaderLibrary.MarkAllDirty();
				mPipelineCache.MarkAllDirty();
				break;
			}
			case ASSET_TYPE::MESH: {
				mDirtyMeshAssets.emplace(id);
				break;
			}
			case ASSET_TYPE::TEXTURE:
			case ASSET_TYPE::MATERIAL:
			case ASSET_TYPE::MATERIAL_INSTANCE: {
				mMaterialsDirty = true;
				break;
			}
			case ASSET_TYPE::POST_FX_CHAIN: {
				mPostFxDirty = true;
				break;
			}
			default: break;
		}
	}

	void RenderDevice::ConsumeDirtyAssets(
		std::unordered_set<AssetID>& outMeshAssets,
		bool&                        outMaterialsDirty,
		bool&                        outPostFxDirty
	) {
		outMeshAssets = std::move(mDirtyMeshAssets);
		mDirtyMeshAssets.clear();
		outMaterialsDirty = mMaterialsDirty;
		outPostFxDirty    = mPostFxDirty;
		mMaterialsDirty   = false;
		mPostFxDirty      = false;
	}

	void RenderDevice::OnResize(const uint32_t width, const uint32_t height) {
		const auto resizeStart = std::chrono::steady_clock::now();
		const RgRegistryDebugStats statsBefore = mRegistry.GetDebugStats();

		mRhiDevice.OnResize(width, height);
		auto& dx = dynamic_cast<Rhi::D3D12Device&>(mRhiDevice);
		mRegistry.OnResize(
			dx.GetSwapChain().GetWidth(),
			dx.GetSwapChain().GetHeight(),
			dx.GetCurrentFrameIndex()
		);

		const RgRegistryDebugStats statsAfter = mRegistry.GetDebugStats();
		const double totalMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - resizeStart
		).count();
		DevMsg(
			kChannel,
			"OnResize done: {}x{} (elapsedMs={:.3f}, activeTex={} -> {}, activeMiB={:.2f} -> {:.2f}, retired={} -> {})",
			width,
			height,
			totalMs,
			statsBefore.activeTextureCount,
			statsAfter.activeTextureCount,
			static_cast<double>(statsBefore.activeTextureBytes) /
				(1024.0 * 1024.0),
			static_cast<double>(statsAfter.activeTextureBytes) /
				(1024.0 * 1024.0),
			statsBefore.retiredResourceCount,
			statsAfter.retiredResourceCount
		);
	}

	RgResourceRegistry& RenderDevice::GetRegistry() {
		return mRegistry;
	}

	void RenderDevice::FlushGpuAndCollectGarbage() {
		auto& dx = dynamic_cast<Rhi::D3D12Device&>(mRhiDevice);
		dx.WaitForGpuIdle();
		mRegistry.CollectGarbage(dx.GetCompletedFenceValue());
	}

	void RenderDevice::HookHotReload() {
		mAssetManager.RegisterReload(
			[this](AssetID id) {
				InvalidateAssetDerivedState(id);
			}
		);
	}
}
