#pragma once
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <dxgiformat.h>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <wrl/client.h>

namespace Unnamed::Rhi {
	class D3D12Device;
}

namespace Unnamed {
	struct TextureAssetData;
}

namespace Unnamed::Render {
	enum class RG_EXTENT_MODE {
		FIXED,
		MATCH_BACK_BUFFER
	};

	/// @brief RgTextureDescは、RenderGraphオブジェクトの生成と初期化に必要な設定を保持します
	struct RgTextureDesc {
		uint32_t width  = 0;
		uint32_t height = 0;

		DXGI_FORMAT resourceFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		bool allowUav = false;
		bool allowRtv = false;
		bool allowDsv = false;

		// ビュー用。 特になければresourceFormatを使う
		std::optional<DXGI_FORMAT> srvFormat;
		std::optional<DXGI_FORMAT> uavFormat;
		std::optional<DXGI_FORMAT> rtvFormat;
		std::optional<DXGI_FORMAT> dsvFormat;

		std::string debugName;

		// これがあるときは、クリアの際にこの色を使う
		std::optional<std::array<float, 4>> optimizedClearColor;
		std::optional<float>                optimizedClearDepth;
		std::optional<uint8_t>              optimizedClearStencil;

		RG_EXTENT_MODE extentMode = RG_EXTENT_MODE::FIXED;
	};

	/// @brief RgRegistryDebugStatsは、RenderGraphの診断用カウンターと集計値を保持します
	struct RgRegistryDebugStats {
		uint32_t activeTextureCount     = 0;
		uint64_t activeTextureBytes     = 0;
		uint32_t retiredResourceCount   = 0;
		uint64_t retiredResourceBytes   = 0;
		uint32_t srvUavActiveSlots      = 0;
		uint32_t rtvActiveSlots         = 0;
		uint32_t dsvActiveSlots         = 0;
		uint32_t cpuSrvUavActiveSlots   = 0;
		uint32_t reusableTextureIdCount = 0;
	};

	/// @brief RgSrvDescriptorTableは、RenderGraph SRV tableのdescriptor先頭、要素数、frame寿命を保持します
	struct RgSrvDescriptorTable {
		uint32_t baseLocal = UINT32_MAX;
		uint32_t count     = 0;

		[[nodiscard]] bool IsValid() const {
			return baseLocal != UINT32_MAX && count > 0;
		}
	};

	/// @brief RgResourceRegistryは、RenderGraphの実装を安定キーで登録し、利用側へ解決します
	class RgResourceRegistry {
	public:
		explicit RgResourceRegistry(Rhi::D3D12Device& dx);

		void SetFramesInFlight(uint32_t framesInFlight);
		void ResetFrame(uint32_t frameIndex);

		uint32_t CreateTexture(const RgTextureDesc& desc);
		uint32_t CreateTextureFromAsset(
			const TextureAssetData& texture, const std::string& debugName
		);
		uint32_t CreateTexture2DFromAsset(
			const TextureAssetData& texture, const std::string& debugName
		);
		void ReleaseTexture(uint32_t textureId);

		[[nodiscard]] ID3D12Resource* GetResource(uint32_t textureId) const;
		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(
			uint32_t textureId
		) const;
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCpu(
			uint32_t textureId
		) const;
		[[nodiscard]] uint64_t GetSrvRevision(uint32_t textureId) const;
		[[nodiscard]] uint64_t GetResourceRevision(uint32_t textureId) const;
		[[nodiscard]] RgSrvDescriptorTable CreateSrvDescriptorTable(
			std::span<const uint32_t> textureIds,
			const std::string&        debugName
		);
		void UpdateSrvDescriptorTable(
			const RgSrvDescriptorTable& table,
			std::span<const uint32_t>   textureIds,
			const std::string&          debugName
		) const;
		void ReleaseSrvDescriptorTable(RgSrvDescriptorTable& table);
		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetSrvDescriptorTableGpu(
			const RgSrvDescriptorTable& table
		) const;
		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetUav(
			uint32_t textureId
		) const;

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetRtvCpu(
			uint32_t textureId
		) const;

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetDsvCpu(
			uint32_t textureId
		) const;

		void OnResize(uint32_t width, uint32_t height, uint32_t frameIndex);
		void CollectGarbage(uint64_t completedFenceValue);
		[[nodiscard]] RgRegistryDebugStats GetDebugStats() const;

	private:
		uint32_t AllocSrvUavSlot();
		uint32_t AllocSrvUavTable(uint32_t count);
		uint32_t AllocRtvSlot();
		uint32_t AllocCpuSlot();
		uint32_t AllocDsvSlot();

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetSrvUavCpuAt(
			uint32_t frameIndex, uint32_t local
		) const;
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCpuSrvUavAt(
			uint32_t local
		) const;
		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetSrvUavGpuAt(
			uint32_t frameIndex, uint32_t local
		) const;

		void RebuildDescriptorLayout();

		/// @brief TexEntryは、RenderGraph textureのD3D12 resource、記述子revision、frame内descriptor indexを所有します
		struct TexEntry {
			RgTextureDesc                          desc;
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			uint64_t                               srvRevision      = 0;
			uint64_t                               resourceRevision = 0;

			// フレーム内インデックス
			uint32_t srvLocal    = UINT32_MAX;
			uint32_t uavLocal    = UINT32_MAX;
			uint32_t rtvLocal    = UINT32_MAX;
			uint32_t dsvLocal    = UINT32_MAX;
			uint32_t srvCpuLocal = UINT32_MAX;

			D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = {};
			D3D12_CPU_DESCRIPTOR_HANDLE rtvCpu = {};
			D3D12_CPU_DESCRIPTOR_HANDLE dsvCpu = {};

			bool isCubeMap = false;
		};

		/// @brief RetiredTextureResourceは、置換済みtexture resourceと安全に解放できるframe番号を保持します
		struct RetiredTextureResource {
			uint64_t                               retireFenceValue = 0;
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			uint64_t                               approxBytes = 0;
			uint32_t                               textureId = 0;
			uint32_t                               srvLocal = UINT32_MAX;
			uint32_t                               uavLocal = UINT32_MAX;
			uint32_t                               rtvLocal = UINT32_MAX;
			uint32_t                               dsvLocal = UINT32_MAX;
			uint32_t                               srvCpuLocal = UINT32_MAX;
			bool                                   releaseTextureId = false;
		};

		uint32_t               AllocateId();
		[[nodiscard]] uint64_t GetSafeRetireFenceValue() const;

		void CreateD3D12Texture(TexEntry& e) const;
		void CreateDescriptors(TexEntry& e);

		Rhi::D3D12Device& mDx;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCpuSrvUavHeap;
		uint32_t mCpuSrvUavDescriptorSize = 0;
		uint32_t mCpuNextSlot = 0;
		uint32_t mCpuHeapCapacity = 0;

		uint32_t mFramesInFlight    = 0;
		uint32_t mCurrentFrameIndex = 0;

		uint32_t mSrvUavPerFrameSlots = 0;
		uint32_t mRtvPerFrameSlots    = 0;

		std::vector<uint32_t> mSrvUavFrameBase;
		std::vector<uint32_t> mRtvFrameBase;

		uint32_t mNextSrvUavLocalGlobal = 0;
		uint32_t mNextRtvLocalGlobal    = 0;

		std::vector<TexEntry> mEntries;    // IDとインデックスは同じ
		uint32_t              mNextId = 1; // 0は無効
		std::vector<uint32_t> mFreeTextureIds;

		uint32_t mSrvUavHeapCapacity = 0;
		uint32_t mRtvCapacity;

		uint32_t mBackBufferWidth  = 0;
		uint32_t mBackBufferHeight = 0;

		std::vector<uint32_t>               mDsvFrameBase;
		uint32_t                            mDsvPerFrameSlots   = 0;
		uint32_t                            mNextDsvLocal       = 0;
		uint32_t                            mNextDsvLocalGlobal = 0;
		uint32_t                            mDsvCapacity        = 0;
		std::vector<RetiredTextureResource> mRetiredResources;
		uint64_t                            mGlobalSrvRevision = 1;

		std::vector<uint32_t>                      mFreeSrvUavLocals;
		std::vector<std::pair<uint32_t, uint32_t>> mFreeSrvUavTableRanges;
		std::vector<uint32_t>                      mFreeRtvLocals;
		std::vector<uint32_t>                      mFreeCpuLocals;
		std::vector<uint32_t>                      mFreeDsvLocals;
	};
}
