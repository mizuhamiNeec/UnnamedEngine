#pragma once
#include <engine/urootsignaturecache/RootSignatureCache.h>

#include <runtime/core/math/Math.h>

#include <runtime/core/Properties.h>

#include "runtime/render/pipeline/UPipelineCache.h"
#include "runtime/render/resources/RenderResourceManager.h"

namespace Unnamed {
	class ShaderLibrary;

	struct TextureParam {
		std::string   name;
		AssetID       asset = kInvalidAssetID;
		TextureHandle tex   = {};
	};

	enum class WatchType : uint8_t {
		MAT, PROGRAM_BODY, PROGRAM_META, TEXTURE
	};

	struct WatchedFile {
		AssetID     id = kInvalidAssetID;
		std::string path;
		uint64_t    size  = 0;
		std::time_t mtime = 0;
		WatchType   type  = WatchType::MAT;
	};

	enum ReloadType : uint32_t {
		RELOAD_NONE    = 0,
		RELOAD_CPU     = 1 << 0,
		RELOAD_TEXTURE = 1 << 1,
	};

	struct UMaterialRuntime {
		AssetID             materialAsset = kInvalidAssetID;
		RootSignatureHandle root          = {};
		PsoHandle           pso           = {};

		struct RootParamIndices {
			UINT srvTable   = UINT_MAX;
			UINT materialCB = UINT_MAX;
			UINT frameCB    = UINT_MAX;
			UINT objectCB   = UINT_MAX;
		};

		RootParamIndices rootParams;

	public:
		bool BuildCPU(
			UAssetManager*      assetManager,
			ShaderLibrary*      shaderLibrary,
			RootSignatureCache* rootSignatureCache,
			UPipelineCache*      pipelineCache,
			GraphicsDevice*     graphicsDevice
		);

		bool RealizeGPU(
			RenderResourceManager*     renderResourceManager,
			ID3D12GraphicsCommandList* commandList
		);

		void InvalidateGPU(
			RenderResourceManager* renderResourceManager,
			ID3D12Fence*           fence,
			uint64_t               value
		);

		uint32_t DetectChanges();

		[[nodiscard]] bool IsGPUReady() const;

		void Apply(
			ID3D12GraphicsCommandList*   commandList,
			const RenderResourceManager* renderResourceManager,
			uint32_t                     backIndex,
			float                        timeSec
		) const;
		void Release(RenderResourceManager* renderResourceManager,
		             ID3D12Fence*           fence, uint64_t value);

		// デバッグ用
		void EnableMipOscillation(bool enable, float speedHz = 1.0f) {
			dbgForceMip  = enable;
			dbgOscillate = enable;
			dbgSpeedHz   = speedHz;
		}

		void SetFixedMip(int mip) {
			dbgForceMip  = true;
			dbgOscillate = false;
			dbgFixedMip  = mip;
		}

		void DisableForcedMip() {
			dbgForceMip = false;
		}

	private:
		std::vector<WatchedFile> mWatchedFiles;

		static bool StartFile(
			const std::string& path, uint64_t& size, std::time_t& mtime
		);

		void ClearWatch();

		void AddWatch(const UAssetManager* assetManager, AssetID id,
		              WatchType            type);

	private:
		struct TextureSlot {
			std::string   name;
			AssetID       asset  = kInvalidAssetID;
			TextureHandle handle = {};
		};

		std::vector<TextureSlot> mTextureSlots;

		// CPU側キャッシュ
		Vec4  mBaseColor = Vec4::one;
		float mMetallic  = 0.0f;

		// CPU側キャッシュ
		std::array<Vec4, 14>                 mExtraCache{};
		std::unordered_map<std::string, int> mNameToSlot;

		// 固定CB
		Microsoft::WRL::ComPtr<ID3D12Resource> mCB = nullptr;
		uint8_t* mCBMapped = nullptr;
		uint32_t mCBStride = 256;
		uint32_t mFramesInFlight = kFrameBufferCount;

		GraphicsDevice* mGraphicsDevice = nullptr;

		bool mGPUReady = false;

		bool  dbgForceMip  = true;
		bool  dbgOscillate = true;
		float dbgSpeedHz   = 1.5f;
		int   dbgFixedMip  = -1;

		mutable int mPrevLoggedMip = -999;

		uint32_t mMainTexMipCount = 1;
	};
}
