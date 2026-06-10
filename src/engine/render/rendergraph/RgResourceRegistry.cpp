#include "RgResourceRegistry.h"

#include <algorithm>
#include <chrono>

#include "d3dx12.h"

#include "core/UnnamedMacro.h"
#include "core/assets/types/TextureAssetData.h"
#include "core/string/StrUtil.h"

#include "engine/rhi/d3d12/D3D12Device.h"
#include "engine/rhi/d3d12/D3D12SwapChain.h"
#include "engine/rhi/d3d12/D3D12Util.h"
#include "engine/unnamed/subsystem/console/Log.h"

using Microsoft::WRL::ComPtr;

namespace Unnamed::Render {
	static constexpr std::string_view kChannel = "RDG";

	namespace {
		[[nodiscard]] uint32_t ApproxBytesPerPixel(const DXGI_FORMAT format) {
			switch (format) {
				case DXGI_FORMAT_R8G8B8A8_UNORM:
				case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return 4;
				case DXGI_FORMAT_R16G16B16A16_FLOAT: return 8;
				case DXGI_FORMAT_R32G8X24_TYPELESS:
				case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
				case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS: return 8;
				default: return 4;
			}
		}

		[[nodiscard]] uint64_t EstimateTextureBytes(ID3D12Resource* resource) {
			if (!resource) {
				return 0;
			}

			const D3D12_RESOURCE_DESC desc = resource->GetDesc();
			if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
				return 0;
			}

			const uint64_t slices = static_cast<uint64_t>(std::max<uint16_t>(
				desc.DepthOrArraySize, 1
			));
			const uint64_t mipLevels = static_cast<uint64_t>(std::max<uint16_t>(
				desc.MipLevels, 1
			));
			const uint64_t pixels =
				desc.Width * static_cast<uint64_t>(desc.Height);
			return pixels * slices * mipLevels *
			       static_cast<uint64_t>(ApproxBytesPerPixel(desc.Format));
		}
	}

	RgResourceRegistry::RgResourceRegistry(Rhi::D3D12Device& dx) : mDx(dx) {
		mEntries.resize(1);

		mSrvUavHeapCapacity = dx.GetSrvUavHeapCapacity();
		mRtvCapacity        = dx.GetRtvHeapCapacity();
		mDsvCapacity        = dx.GetDsvHeapCapacity();

		SetFramesInFlight(2);
		RebuildDescriptorLayout();

		if (const auto* sc = dx.GetD3D12SwapChain()) {
			mBackBufferWidth  = sc->GetWidth();
			mBackBufferHeight = sc->GetHeight();
		}

		auto* device     = mDx.GetDevice();
		mCpuHeapCapacity = 8192; // とりあえず適当な値

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = mCpuHeapCapacity;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		Rhi::Throw(
			device->CreateDescriptorHeap(
				&desc, IID_PPV_ARGS(mCpuSrvUavHeap.ReleaseAndGetAddressOf())
			)
		);

		mCpuSrvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);
	}

	void RgResourceRegistry::SetFramesInFlight(const uint32_t framesInFlight) {
		// 最佁Eにする
		mFramesInFlight = std::max(2u, framesInFlight);
		mSrvUavFrameBase.resize(mFramesInFlight);
		mRtvFrameBase.resize(mFramesInFlight);
		mDsvFrameBase.resize(mFramesInFlight);
		RebuildDescriptorLayout();
	}

	void RgResourceRegistry::ResetFrame(const uint32_t frameIndex) {
		mCurrentFrameIndex = frameIndex % mFramesInFlight;
		mNextDsvLocal      = 0;
	}

	uint32_t RgResourceRegistry::CreateTexture(const RgTextureDesc& desc) {
		const uint32_t id = AllocateId();

		if (mEntries.size() <= id) {
			mEntries.resize(id + 1);
		}

		auto resolved = desc;
		if (resolved.extentMode == RG_EXTENT_MODE::MATCH_BACK_BUFFER) {
			resolved.width  = mBackBufferWidth;
			resolved.height = mBackBufferHeight;
			Msg(
				kChannel, "テクスチャ {} はバックバッファに合わせてリサイズされます。 {}x{}",
				desc.debugName, resolved.width, resolved.height
			);
		}

		auto& e     = mEntries[id];
		e.desc      = resolved;
		e.isCubeMap = false;

		CreateD3D12Texture(e);
		CreateDescriptors(e);

		return id;
	}

	uint32_t RgResourceRegistry::CreateTextureFromAsset(
		const TextureAssetData& texture, const std::string& debugName
	) {
		if (texture.width == 0 || texture.height == 0) {
			return 0;
		}

		const uint32_t mipLevels = texture.mipLevels > 0 ?
			                           texture.mipLevels :
			                           static_cast<uint32_t>(texture.mips.
				                           size());
		if (mipLevels == 0) {
			return 0;
		}

		const uint32_t arraySize = std::max<uint32_t>(texture.arraySize, 1);
		const uint32_t expectedSubresources = mipLevels * arraySize;

		std::vector<const TextureSubresource*> subresourceInfos(
			expectedSubresources, nullptr
		);
		std::vector<TextureSubresource> legacySubresources;
		if (!texture.subresources.empty()) {
			for (const TextureSubresource& subresource : texture.subresources) {
				if (
					subresource.mipLevel >= mipLevels ||
					subresource.arraySlice >= arraySize
				) {
					continue;
				}
				const uint32_t index = subresource.mipLevel +
				                       subresource.arraySlice * mipLevels;
				subresourceInfos[index] = &subresource;
			}
		} else {
			legacySubresources.reserve(mipLevels);
			for (uint32_t mipLevel = 0; mipLevel < mipLevels; ++mipLevel) {
				if (mipLevel >= texture.mips.size()) {
					break;
				}
				const TextureMip& mip = texture.mips[mipLevel];
				if (mip.bytes.empty()) {
					continue;
				}
				TextureSubresource legacySubresource = {};
				legacySubresource.width              = mip.width;
				legacySubresource.height             = mip.height;
				legacySubresource.rowPitch           = mip.rowPitch;
				legacySubresource.slicePitch         = mip.bytes.size();
				legacySubresource.mipLevel           = mipLevel;
				legacySubresource.arraySlice         = 0;
				legacySubresource.bytes              = mip.bytes;
				legacySubresources.emplace_back(std::move(legacySubresource));
				subresourceInfos[mipLevel] = &legacySubresources.back();
			}
		}

		if (
			std::ranges::any_of(
				subresourceInfos,
				[](const TextureSubresource* subresource) {
					return subresource == nullptr;
				}
			)) {
			Warning(
				kChannel,
				"テクスチャのサブリソースが不足しているため作成できません: {}",
				debugName
			);
			return 0;
		}

		DXGI_FORMAT textureFormat = texture.format;
		if (textureFormat == DXGI_FORMAT_UNKNOWN) {
			textureFormat = texture.isSRGB ?
				                DXGI_FORMAT_R8G8B8A8_UNORM_SRGB :
				                DXGI_FORMAT_R8G8B8A8_UNORM;
		}

		const uint32_t id = AllocateId();
		if (mEntries.size() <= id) {
			mEntries.resize(id + 1);
		}

		auto& e = mEntries[id];
		e.desc  = RgTextureDesc{
			.width          = texture.width,
			.height         = texture.height,
			.resourceFormat = textureFormat,
			.allowUav       = false,
			.allowRtv       = false,
			.allowDsv       = false,
			.debugName      = debugName,
			.extentMode     = RG_EXTENT_MODE::FIXED,
		};

		auto* device = mDx.GetDevice();

		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Width               = e.desc.width;
		texDesc.Height              = e.desc.height;
		texDesc.DepthOrArraySize    = static_cast<uint16_t>(arraySize);
		texDesc.MipLevels           = static_cast<uint16_t>(mipLevels);
		texDesc.Format              = e.desc.resourceFormat;
		texDesc.SampleDesc.Count    = 1;
		texDesc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags               = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES heap = {};
		heap.Type                  = D3D12_HEAP_TYPE_DEFAULT;

		Rhi::Throw(
			device->CreateCommittedResource(
				&heap,
				D3D12_HEAP_FLAG_NONE,
				&texDesc,
				D3D12_RESOURCE_STATE_COMMON,
				nullptr,
				IID_PPV_ARGS(e.resource.ReleaseAndGetAddressOf())
			)
		);

		if (!debugName.empty()) {
			e.resource->SetName(StrUtil::ToWString(debugName).c_str());
		}
		e.isCubeMap = texture.isCubeMap;

		if (!subresourceInfos.empty()) {
			auto& up = mDx.GetUploadContext();
			up.Begin();
			auto* cmd = up.GetCommandList();

			D3D12_RESOURCE_DESC rd = e.resource->GetDesc();
			const UINT subresourceCount = static_cast<UINT>(std::min<uint32_t>(
				expectedSubresources,
				static_cast<uint32_t>(rd.MipLevels) *
				static_cast<uint32_t>(rd.DepthOrArraySize)
			));
			uint64_t                                        requiredBytes = 0;
			std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(
				subresourceCount
			);
			std::vector<UINT>     numRows(subresourceCount);
			std::vector<uint64_t> rowSizeInBytes(subresourceCount);
			device->GetCopyableFootprints(
				&rd,
				0,
				subresourceCount,
				0,
				footprints.data(),
				numRows.data(),
				rowSizeInBytes.data(),
				&requiredBytes
			);

			ComPtr<ID3D12Resource> upload;
			D3D12_HEAP_PROPERTIES  upHeap = {};
			upHeap.Type                   = D3D12_HEAP_TYPE_UPLOAD;
			D3D12_RESOURCE_DESC upDesc    = {};
			upDesc.Dimension              = D3D12_RESOURCE_DIMENSION_BUFFER;
			upDesc.Width                  = requiredBytes;
			upDesc.Height                 = 1;
			upDesc.DepthOrArraySize       = 1;
			upDesc.MipLevels              = 1;
			upDesc.SampleDesc.Count       = 1;
			upDesc.Layout                 = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			upDesc.Flags                  = D3D12_RESOURCE_FLAG_NONE;

			Rhi::Throw(
				device->CreateCommittedResource(
					&upHeap,
					D3D12_HEAP_FLAG_NONE,
					&upDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(upload.ReleaseAndGetAddressOf())
				)
			);

			void*       map       = nullptr;
			D3D12_RANGE readRange = {.Begin = 0, .End = 0};
			Rhi::Throw(upload->Map(0, &readRange, &map));
			bool copyValid = true;
			for (UINT subresourceIndex = 0; subresourceIndex < subresourceCount;
			     ++subresourceIndex) {
				const TextureSubresource* subresource =
					subresourceInfos[subresourceIndex];
				if (!subresource) {
					continue;
				}
				const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint =
					footprints[subresourceIndex];
				for (UINT y = 0; y < numRows[subresourceIndex]; ++y) {
					const size_t srcOffset =
						static_cast<size_t>(y) * subresource->rowPitch;
					const uint8_t* src = srcOffset < subresource->bytes.size() ?
						                     subresource->bytes.data() +
						                     srcOffset :
						                     nullptr;
					uint8_t* dst =
						static_cast<uint8_t*>(map) + footprint.Offset +
						static_cast<size_t>(y) *
						footprint.Footprint.RowPitch;
					const size_t rowBytes = static_cast<size_t>(
						rowSizeInBytes[subresourceIndex]
					);
					const uint64_t dstBegin =
						footprint.Offset +
						static_cast<uint64_t>(y) *
						footprint.Footprint.RowPitch;
					const uint64_t dstEnd = dstBegin + rowBytes;
					if (dstEnd > requiredBytes) {
						Warning(
							kChannel,
							"テクスチャのアップロード範囲が不正です: {}, subresource={}, row={}",
							debugName,
							subresourceIndex,
							y
						);
						copyValid = false;
						break;
					}
					size_t copyBytes = 0;
					if (src) {
						copyBytes = std::min<size_t>(
							rowBytes, subresource->bytes.size() - srcOffset
						);
					}
					if (copyBytes > 0) {
						memcpy(dst, src, copyBytes);
					}
					if (copyBytes < rowBytes) {
						std::memset(dst + copyBytes, 0, rowBytes - copyBytes);
					}
				}
				if (!copyValid) {
					break;
				}
			}
			upload->Unmap(0, nullptr);
			if (!copyValid) {
				up.EndAndSubmitAndWait();
				ReleaseTexture(id);
				return 0;
			}

			auto toCopy = CD3DX12_RESOURCE_BARRIER::Transition(
				e.resource.Get(),
				D3D12_RESOURCE_STATE_COMMON,
				D3D12_RESOURCE_STATE_COPY_DEST
			);
			cmd->ResourceBarrier(1, &toCopy);

			for (UINT subresourceIndex = 0; subresourceIndex < subresourceCount;
			     ++subresourceIndex) {
				D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
				srcLoc.pResource = upload.Get();
				srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
				srcLoc.PlacedFootprint = footprints[subresourceIndex];

				D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
				dstLoc.pResource = e.resource.Get();
				dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				dstLoc.SubresourceIndex = subresourceIndex;

				cmd->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
			}

			auto toCommon = CD3DX12_RESOURCE_BARRIER::Transition(
				e.resource.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_COMMON
			);
			cmd->ResourceBarrier(1, &toCommon);

			up.EndAndSubmitAndWait();
		}

		++e.resourceRevision;
		CreateDescriptors(e);
		return id;
	}

	uint32_t RgResourceRegistry::CreateTexture2DFromAsset(
		const TextureAssetData& texture, const std::string& debugName
	) {
		return CreateTextureFromAsset(texture, debugName);
	}

	void RgResourceRegistry::ReleaseTexture(const uint32_t textureId) {
		if (textureId == 0 || textureId >= mEntries.size()) {
			return;
		}

		auto& e = mEntries[textureId];
		if (
			!e.resource &&
			e.srvLocal == UINT32_MAX &&
			e.uavLocal == UINT32_MAX &&
			e.rtvLocal == UINT32_MAX &&
			e.dsvLocal == UINT32_MAX &&
			e.srvCpuLocal == UINT32_MAX
		) {
			return;
		}

		RetiredTextureResource retired = {};
		retired.retireFenceValue       = std::max(
			GetSafeRetireFenceValue(), mDx.GetNextSignalFenceValue()
		);
		retired.approxBytes      = EstimateTextureBytes(e.resource.Get());
		retired.resource         = e.resource;
		retired.textureId        = textureId;
		retired.srvLocal         = e.srvLocal;
		retired.uavLocal         = e.uavLocal;
		retired.rtvLocal         = e.rtvLocal;
		retired.dsvLocal         = e.dsvLocal;
		retired.srvCpuLocal      = e.srvCpuLocal;
		retired.releaseTextureId = true;
		mRetiredResources.emplace_back(std::move(retired));

		e.resource.Reset();
		e.desc        = {};
		e.srvLocal    = UINT32_MAX;
		e.uavLocal    = UINT32_MAX;
		e.rtvLocal    = UINT32_MAX;
		e.dsvLocal    = UINT32_MAX;
		e.srvCpuLocal = UINT32_MAX;
		e.srvCpu      = {};
		e.rtvCpu      = {};
		e.dsvCpu      = {};
		e.isCubeMap   = false;
	}

	ID3D12Resource* RgResourceRegistry::GetResource(
		const uint32_t textureId
	) const {
		if (textureId == 0 || textureId >= mEntries.size()) {
			return nullptr;
		}
		return mEntries[textureId].resource.Get();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetSrv(
		const uint32_t textureId
	) const {
		if (textureId == 0 || textureId >= mEntries.size()) {
			return {};
		}
		const auto& e = mEntries[textureId];
		if (e.srvLocal == UINT32_MAX) {
			return {};
		}
		return GetSrvUavGpuAt(mCurrentFrameIndex, e.srvLocal);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetSrvCpu(
		const uint32_t textureId
	) const {
		if (textureId == 0 || textureId >= mEntries.size()) {
			return {};
		}
		const auto& e = mEntries[textureId];
		if (e.srvCpu.ptr == 0) {
			return {};
		}
		return e.srvCpu;
	}

	uint64_t RgResourceRegistry::GetSrvRevision(
		const uint32_t textureId
	) const {
		if (textureId == 0 || textureId >= mEntries.size()) {
			return 0;
		}
		return mEntries[textureId].srvRevision;
	}

	uint64_t RgResourceRegistry::GetResourceRevision(
		const uint32_t textureId
	) const {
		if (textureId == 0 || textureId >= mEntries.size()) {
			return 0;
		}
		return mEntries[textureId].resourceRevision;
	}

	RgSrvDescriptorTable RgResourceRegistry::CreateSrvDescriptorTable(
		const std::span<const uint32_t> textureIds,
		const std::string&              debugName
	) {
		RgSrvDescriptorTable table;
		table.count     = static_cast<uint32_t>(textureIds.size());
		table.baseLocal = AllocSrvUavTable(table.count);
		UpdateSrvDescriptorTable(table, textureIds, debugName);
		return table;
	}

	void RgResourceRegistry::UpdateSrvDescriptorTable(
		const RgSrvDescriptorTable&     table,
		const std::span<const uint32_t> textureIds,
		const std::string&              debugName
	) const {
		if (!table.IsValid() || textureIds.size() != table.count) {
			Fatal(
				kChannel,
				"Invalid SRV descriptor table update: name={}, base={}, tableCount={}, textureCount={}",
				debugName,
				table.baseLocal,
				table.count,
				textureIds.size()
			);
			return;
		}

		auto* device = mDx.GetDevice();
		for (uint32_t i = 0; i < table.count; ++i) {
			const D3D12_CPU_DESCRIPTOR_HANDLE src = GetSrvCpu(textureIds[i]);
			if (src.ptr == 0) {
				Fatal(
					kChannel,
					"Invalid SRV source for descriptor table: name={}, textureId={}, index={}",
					debugName,
					textureIds[i],
					i
				);
				return;
			}
			for (uint32_t fi = 0; fi < mFramesInFlight; ++fi) {
				const auto dst = GetSrvUavCpuAt(fi, table.baseLocal + i);
				device->CopyDescriptorsSimple(
					1,
					dst,
					src,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
			}
		}
	}

	void RgResourceRegistry::ReleaseSrvDescriptorTable(
		RgSrvDescriptorTable& table
	) {
		if (!table.IsValid()) {
			return;
		}
		mFreeSrvUavTableRanges.emplace_back(table.baseLocal, table.count);
		table = {};
	}

	D3D12_GPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetSrvDescriptorTableGpu(
		const RgSrvDescriptorTable& table
	) const {
		if (!table.IsValid()) {
			return {};
		}
		return GetSrvUavGpuAt(mCurrentFrameIndex, table.baseLocal);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetUav(
		const uint32_t textureId
	) const {
		if (textureId == 0 || textureId >= mEntries.size()) {
			return {};
		}
		const auto& e = mEntries[textureId];
		if (e.uavLocal == UINT32_MAX) {
			return {};
		}
		return GetSrvUavGpuAt(mCurrentFrameIndex, e.uavLocal);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetRtvCpu(
		const uint32_t textureId
	) const {
		if (textureId == 0 || textureId >= mEntries.size()) {
			return {};
		}
		const auto& e = mEntries[textureId];
		if (e.rtvLocal == UINT32_MAX) {
			return {};
		}

		auto*      rtvHeap = mDx.GetRtvHeap();
		const auto cpuBase = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		const UINT inc     = mDx.GetRtvDescriptorSize();

		const uint32_t slot = mRtvFrameBase[mCurrentFrameIndex] + e.rtvLocal;

		D3D12_CPU_DESCRIPTOR_HANDLE h = cpuBase;
		h.ptr                         += static_cast<SIZE_T>(slot) * inc;
		return h;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetDsvCpu(
		const uint32_t textureId
	) const {
		if (textureId == 0 || textureId >= mEntries.size()) {
			return {};
		}
		return mEntries[textureId].dsvCpu;
	}

	void RgResourceRegistry::OnResize(
		const uint32_t width, const uint32_t height, const uint32_t frameIndex
	) {
		const auto resizeStart = std::chrono::steady_clock::now();
		const RgRegistryDebugStats statsBefore = GetDebugStats();

		mBackBufferWidth  = width;
		mBackBufferHeight = height;

		// フレームリセット
		ResetFrame(frameIndex);

		uint32_t recreatedCount = 0;
		for (auto& e : mEntries) {
			if (!e.resource) {
				continue;
			}

			if (e.desc.extentMode != RG_EXTENT_MODE::MATCH_BACK_BUFFER) {
				continue;
			}

			if (e.desc.width == width && e.desc.height == height) {
				continue;
			}

			e.desc.width  = width;
			e.desc.height = height;

			CreateD3D12Texture(e);
			CreateDescriptors(e);
			++recreatedCount;
		}

		const RgRegistryDebugStats statsAfter = GetDebugStats();
		const double resizeMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - resizeStart
		).count();
		DevMsg(
			kChannel,
			"OnResize {}x{}: recreatedMatchBackBuffer={}, elapsedMs={:.3f}, activeTex={} -> {}, retired={} -> {}, retiredMiB={:.2f} -> {:.2f}, srvUavSlots={} -> {}, rtvSlots={} -> {}, dsvSlots={} -> {}, cpuSrvUavSlots={} -> {}",
			width,
			height,
			recreatedCount,
			resizeMs,
			statsBefore.activeTextureCount,
			statsAfter.activeTextureCount,
			statsBefore.retiredResourceCount,
			statsAfter.retiredResourceCount,
			static_cast<double>(statsBefore.retiredResourceBytes) /
			(1024.0 * 1024.0),
			static_cast<double>(statsAfter.retiredResourceBytes) /
			(1024.0 * 1024.0),
			statsBefore.srvUavActiveSlots,
			statsAfter.srvUavActiveSlots,
			statsBefore.rtvActiveSlots,
			statsAfter.rtvActiveSlots,
			statsBefore.dsvActiveSlots,
			statsAfter.dsvActiveSlots,
			statsBefore.cpuSrvUavActiveSlots,
			statsAfter.cpuSrvUavActiveSlots
		);
	}

	void RgResourceRegistry::CollectGarbage(
		const uint64_t completedFenceValue
	) {
		uint32_t reclaimedCount = 0;
		uint64_t reclaimedBytes = 0;
		std::erase_if(mRetiredResources,
		              [this, completedFenceValue, &reclaimedCount, &
			              reclaimedBytes](
		              const RetiredTextureResource& retired
	              ) {
			              const bool ready = retired.retireFenceValue == 0 ||
			                                 retired.retireFenceValue <=
			                                 completedFenceValue;
			              if (!ready) {
				              return false;
			              }

			              if (retired.srvLocal != UINT32_MAX) {
				              mFreeSrvUavLocals.emplace_back(retired.srvLocal);
			              }
			              if (retired.uavLocal != UINT32_MAX) {
				              mFreeSrvUavLocals.emplace_back(retired.uavLocal);
			              }
			              if (retired.rtvLocal != UINT32_MAX) {
				              mFreeRtvLocals.emplace_back(retired.rtvLocal);
			              }
			              if (retired.dsvLocal != UINT32_MAX) {
				              mFreeDsvLocals.emplace_back(retired.dsvLocal);
			              }
			              if (retired.srvCpuLocal != UINT32_MAX) {
				              mFreeCpuLocals.emplace_back(retired.srvCpuLocal);
			              }
			              if (retired.releaseTextureId && retired.textureId !=
			                  0) {
				              mFreeTextureIds.emplace_back(retired.textureId);
			              }
			              ++reclaimedCount;
			              reclaimedBytes += retired.approxBytes;
			              return true;
		              }
		);

		if (reclaimedCount > 0) {
			uint64_t pendingBytes = 0;
			for (const auto& pending : mRetiredResources) {
				pendingBytes += pending.approxBytes;
			}

			DevMsg(
				kChannel,
				"Retired resources reclaimed: count={}, approxMiB={:.2f}, pendingCount={}, pendingMiB={:.2f}",
				reclaimedCount,
				static_cast<double>(reclaimedBytes) / (1024.0 * 1024.0),
				mRetiredResources.size(),
				static_cast<double>(pendingBytes) / (1024.0 * 1024.0)
			);
		}
	}

	RgRegistryDebugStats RgResourceRegistry::GetDebugStats() const {
		RgRegistryDebugStats stats = {};
		for (const auto& entry : mEntries) {
			if (!entry.resource) {
				continue;
			}
			++stats.activeTextureCount;
			stats.activeTextureBytes += EstimateTextureBytes(
				entry.resource.Get());
		}
		for (const auto& retired : mRetiredResources) {
			++stats.retiredResourceCount;
			stats.retiredResourceBytes += retired.approxBytes;
		}

		stats.srvUavActiveSlots = mNextSrvUavLocalGlobal >
		                          mFreeSrvUavLocals.size() ?
			                          mNextSrvUavLocalGlobal -
			                          static_cast<uint32_t>(mFreeSrvUavLocals.
				                          size()) :
			                          0;
		stats.rtvActiveSlots = mNextRtvLocalGlobal > mFreeRtvLocals.size() ?
			                       mNextRtvLocalGlobal -
			                       static_cast<uint32_t>(mFreeRtvLocals.
				                       size()) :
			                       0;
		stats.dsvActiveSlots = mNextDsvLocalGlobal > mFreeDsvLocals.size() ?
			                       mNextDsvLocalGlobal -
			                       static_cast<uint32_t>(mFreeDsvLocals.
				                       size()) :
			                       0;
		stats.cpuSrvUavActiveSlots = mCpuNextSlot > mFreeCpuLocals.size() ?
			                             mCpuNextSlot -
			                             static_cast<uint32_t>(mFreeCpuLocals.
				                             size()) :
			                             0;
		stats.reusableTextureIdCount = static_cast<uint32_t>(
			mFreeTextureIds.size()
		);
		return stats;
	}

	uint32_t RgResourceRegistry::AllocSrvUavSlot() {
		if (!mFreeSrvUavLocals.empty()) {
			const uint32_t local = mFreeSrvUavLocals.back();
			mFreeSrvUavLocals.pop_back();
			return local;
		}

		const uint32_t local = mNextSrvUavLocalGlobal++;
		if (local >= mSrvUavPerFrameSlots) {
			Fatal(
				kChannel, "SRV/UAV heap slots exhausted: local={}, perFrame={}",
				local, mSrvUavPerFrameSlots
			);
			UASSERT(false);
		}
		return local;
	}

	uint32_t RgResourceRegistry::AllocSrvUavTable(const uint32_t count) {
		if (count == 0) {
			return UINT32_MAX;
		}
		for (auto it = mFreeSrvUavTableRanges.begin();
		     it != mFreeSrvUavTableRanges.end();
		     ++it) {
			auto& [base, available] = *it;
			if (available < count) {
				continue;
			}
			const uint32_t local = base;
			base                 += count;
			available            -= count;
			if (available == 0) {
				mFreeSrvUavTableRanges.erase(it);
			}
			return local;
		}
		const uint32_t local   = mNextSrvUavLocalGlobal;
		mNextSrvUavLocalGlobal += count;
		if (mNextSrvUavLocalGlobal > mSrvUavPerFrameSlots) {
			Fatal(
				kChannel,
				"SRV/UAV heap table slots exhausted: local={}, count={}, perFrame={}",
				local,
				count,
				mSrvUavPerFrameSlots
			);
			UASSERT(false);
		}
		return local;
	}

	uint32_t RgResourceRegistry::AllocRtvSlot() {
		if (!mFreeRtvLocals.empty()) {
			const uint32_t local = mFreeRtvLocals.back();
			mFreeRtvLocals.pop_back();
			return local;
		}

		const uint32_t local = mNextRtvLocalGlobal++;
		if (local >= mRtvPerFrameSlots) {
			Fatal(
				kChannel, "RTV heap slots exhausted: local={}, perFrame={}",
				local, mRtvPerFrameSlots
			);
			UASSERT(false);
		}
		return local;
	}

	uint32_t RgResourceRegistry::AllocCpuSlot() {
		if (!mFreeCpuLocals.empty()) {
			const uint32_t slot = mFreeCpuLocals.back();
			mFreeCpuLocals.pop_back();
			return slot;
		}

		const uint32_t slot = mCpuNextSlot++;
		if (slot >= mCpuHeapCapacity) {
			Fatal(
				kChannel,
				"CPU SRV/UAV heap slots exhausted: slot={}, capacity={}",
				slot, mCpuHeapCapacity
			);
			UASSERT(false && "CPU SRV/UAV heap slots exhausted.");
		}
		return slot;
	}

	uint32_t RgResourceRegistry::AllocDsvSlot() {
		if (!mFreeDsvLocals.empty()) {
			const uint32_t slot = mFreeDsvLocals.back();
			mFreeDsvLocals.pop_back();
			return slot;
		}

		const uint32_t slot = mNextDsvLocalGlobal++;
		if (slot >= mDsvCapacity) {
			Fatal(
				kChannel,
				"DSV heap slots exhausted: slot={}, capacity={}",
				slot, mDsvCapacity
			);
		}
		return slot;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetSrvUavCpuAt(
		const uint32_t frameIndex, const uint32_t local
	) const {
		auto*      heap = mDx.GetSrvUavHeap();
		const auto base = heap->GetCPUDescriptorHandleForHeapStart();
		const UINT inc  = mDx.GetDevice()->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

		const uint32_t              slot = mSrvUavFrameBase[frameIndex] + local;
		D3D12_CPU_DESCRIPTOR_HANDLE h    = base;
		h.ptr                            += static_cast<SIZE_T>(slot) * inc;
		return h;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetCpuSrvUavAt(
		const uint32_t local
	) const {
		const auto base = mCpuSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_CPU_DESCRIPTOR_HANDLE h = base;
		h.ptr += static_cast<SIZE_T>(local) * mCpuSrvUavDescriptorSize;
		return h;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE RgResourceRegistry::GetSrvUavGpuAt(
		const uint32_t frameIndex, const uint32_t local
	) const {
		auto*      heap = mDx.GetSrvUavHeap();
		const auto base = heap->GetGPUDescriptorHandleForHeapStart();
		const UINT inc  = mDx.GetDevice()->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

		const uint32_t              slot = mSrvUavFrameBase[frameIndex] + local;
		D3D12_GPU_DESCRIPTOR_HANDLE h    = base;
		h.ptr                            += static_cast<SIZE_T>(slot) * inc;
		return h;
	}

	void RgResourceRegistry::RebuildDescriptorLayout() {
		const uint32_t srvUavCap = mDx.GetSrvUavHeapCapacity();
		const uint32_t rtvCap    = mDx.GetRtvHeapCapacity();
		const uint32_t dsvCap    = mDx.GetDsvHeapCapacity();

		// Reserve at least one slot per frame.
		mSrvUavPerFrameSlots = std::max(1u, srvUavCap / mFramesInFlight);
		mRtvPerFrameSlots    = std::max(1u, rtvCap / mFramesInFlight);
		mDsvPerFrameSlots    = std::max(1u, dsvCap / mFramesInFlight);

		for (uint32_t i = 0; i < mFramesInFlight; ++i) {
			mSrvUavFrameBase[i] = i * mSrvUavPerFrameSlots;
			mRtvFrameBase[i]    = i * mRtvPerFrameSlots;
			mDsvFrameBase[i]    = i * mDsvPerFrameSlots;
		}

		DevMsg(
			kChannel,
			"DescriptorLayout: frames={}, srvUavCap={}, perFrame={}, rtvCap={}, perFrame={}",
			mFramesInFlight, srvUavCap, mSrvUavPerFrameSlots, rtvCap,
			mRtvPerFrameSlots
		);
	}

	uint32_t RgResourceRegistry::AllocateId() {
		if (!mFreeTextureIds.empty()) {
			const uint32_t id = mFreeTextureIds.back();
			mFreeTextureIds.pop_back();
			return id;
		}
		return mNextId++;
	}

	uint64_t RgResourceRegistry::GetSafeRetireFenceValue() const {
		uint64_t       retireFenceValue = 0;
		const uint32_t framesInFlight   = mDx.GetFramesInFlight();
		for (uint32_t frameIndex = 0; frameIndex < framesInFlight; ++
		     frameIndex) {
			retireFenceValue = std::max(
				retireFenceValue,
				mDx.GetLastSubmittedFenceValue(frameIndex)
			);
		}
		return retireFenceValue;
	}

	void RgResourceRegistry::CreateD3D12Texture(TexEntry& e) const {
		auto* device = mDx.GetDevice();

		const DXGI_FORMAT resourceFormat = e.desc.resourceFormat;

		// ---- validation ----
		if (e.desc.allowDsv) {
			if (!e.desc.dsvFormat.has_value()) {
				Fatal(
					kChannel,
					"Depth texture {} requires dsvFormat.",
					e.desc.debugName
				);
				UASSERT(false);
			}

			if (
				resourceFormat != DXGI_FORMAT_R32_TYPELESS &&
				resourceFormat != DXGI_FORMAT_R32G8X24_TYPELESS &&
				resourceFormat != DXGI_FORMAT_R24G8_TYPELESS &&
				resourceFormat != DXGI_FORMAT_R16_TYPELESS
			) {
				Warning(
					kChannel,
					"Depth texture {} is not typeless. SRV reads may fail.",
					e.desc.debugName
				);
			}
		}

		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width               = e.desc.width;
		desc.Height              = e.desc.height;
		desc.DepthOrArraySize    = 1;
		desc.MipLevels           = 1;
		desc.Format              = resourceFormat;
		desc.SampleDesc.Count    = 1;
		desc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;

		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		// UAV、RTV、DSVの使用フラグを設定
		if (e.desc.allowUav) {
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}
		if (e.desc.allowRtv) {
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		}
		if (e.desc.allowDsv) {
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		}

		D3D12_HEAP_PROPERTIES heap = {};
		heap.Type                  = D3D12_HEAP_TYPE_DEFAULT;

		// 最適化されたクリアカラー/深度がある場合は、リソース作成時にクリア値を指定する。
		const D3D12_CLEAR_VALUE* clearPtr = nullptr;
		D3D12_CLEAR_VALUE        clear    = {};

		if (e.desc.allowRtv && e.desc.optimizedClearColor.has_value()) {
			clear.Format = e.desc.rtvFormat.value_or(e.desc.resourceFormat);
			for (int i = 0; i < 4; ++i) {
				clear.Color[i] = (*e.desc.optimizedClearColor)[i];
			}
			clearPtr = &clear;
		}

		if (e.desc.allowDsv) {
			clear.Format             = e.desc.dsvFormat.value();
			clear.DepthStencil.Depth = e.desc.optimizedClearDepth.
			                             value_or(1.0f);
			clear.DepthStencil.Stencil = e.desc.optimizedClearStencil.value_or(
				0
			);
			clearPtr = &clear;
		}

		auto* self = const_cast<RgResourceRegistry*>(this);
		if (e.resource) {
			RetiredTextureResource retired = {};
			retired.retireFenceValue       = std::max(
				self->GetSafeRetireFenceValue(), mDx.GetNextSignalFenceValue()
			);
			retired.approxBytes = EstimateTextureBytes(e.resource.Get());
			retired.resource    = e.resource;
			self->mRetiredResources.emplace_back(std::move(retired));
			e.resource.Reset();
		}

		// Create resource
		Rhi::Throw(
			device->CreateCommittedResource(
				&heap,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_COMMON,
				clearPtr,
				IID_PPV_ARGS(e.resource.ReleaseAndGetAddressOf())
			)
		);

		// Set debug name
		if (!e.desc.debugName.empty()) {
			e.resource->SetName(
				StrUtil::ToWString(e.desc.debugName).c_str()
			);
		}

		++e.resourceRevision;
	}

	void RgResourceRegistry::CreateDescriptors(TexEntry& e) {
		auto* device = mDx.GetDevice();

		if (e.srvLocal == UINT32_MAX) {
			e.srvLocal = AllocSrvUavSlot();
		}
		if (e.srvCpuLocal == UINT32_MAX) {
			e.srvCpuLocal = AllocCpuSlot();
		}
		if (e.desc.allowUav && e.uavLocal == UINT32_MAX) {
			e.uavLocal = AllocSrvUavSlot();
		}
		if (e.desc.allowRtv && e.rtvLocal == UINT32_MAX) {
			e.rtvLocal = AllocRtvSlot();
		}
		if (e.desc.allowDsv && e.dsvLocal == UINT32_MAX) {
			e.dsvLocal = AllocDsvSlot();
		}

		const bool isDepth = e.desc.allowDsv;

		DXGI_FORMAT srvFormat = e.desc.srvFormat.value_or(
			e.desc.resourceFormat
		);
		if (isDepth) {
			if (!e.desc.srvFormat.has_value()) {
				Fatal(
					kChannel,
					"Depth texture {} requires srvFormat.",
					e.desc.debugName
				);
				UASSERT(false);
			}
			srvFormat = e.desc.srvFormat.value();
		}
		const D3D12_RESOURCE_DESC resourceDesc = e.resource->GetDesc();

		D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
		srv.Format                          = srvFormat;
		srv.Shader4ComponentMapping         =
			D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		if (e.isCubeMap) {
			srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srv.TextureCube.MostDetailedMip = 0;
			srv.TextureCube.MipLevels = resourceDesc.MipLevels;
			srv.TextureCube.ResourceMinLODClamp = 0.0f;
		} else if (resourceDesc.DepthOrArraySize > 1) {
			srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srv.Texture2DArray.MostDetailedMip = 0;
			srv.Texture2DArray.MipLevels = resourceDesc.MipLevels;
			srv.Texture2DArray.FirstArraySlice = 0;
			srv.Texture2DArray.ArraySize = resourceDesc.DepthOrArraySize;
			srv.Texture2DArray.PlaneSlice = 0;
			srv.Texture2DArray.ResourceMinLODClamp = 0.0f;
		} else {
			srv.ViewDimension       = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv.Texture2D.MipLevels = resourceDesc.MipLevels;
		}

		D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
		uav.Format                           = e.desc.resourceFormat;
		uav.ViewDimension                    = D3D12_UAV_DIMENSION_TEXTURE2D;

		e.srvCpu = GetCpuSrvUavAt(e.srvCpuLocal);
		device->CreateShaderResourceView(
			e.resource.Get(), &srv, e.srvCpu
		);

		for (uint32_t fi = 0; fi < mFramesInFlight; ++fi) {
			const auto cpuSrv = GetSrvUavCpuAt(fi, e.srvLocal);
			device->CreateShaderResourceView(
				e.resource.Get(), &srv, cpuSrv
			);

			if (e.desc.allowUav) {
				const auto cpuUav = GetSrvUavCpuAt(fi, e.uavLocal);
				device->CreateUnorderedAccessView(
					e.resource.Get(), nullptr, &uav, cpuUav
				);
			}
		}

		if (e.desc.allowRtv) {
			auto*      rtvHeap = mDx.GetRtvHeap();
			const auto cpuBase = rtvHeap->GetCPUDescriptorHandleForHeapStart();
			const UINT inc     = mDx.GetRtvDescriptorSize();

			for (uint32_t fi = 0; fi < mFramesInFlight; ++fi) {
				const uint32_t slot = mRtvFrameBase[fi] + e.rtvLocal;
				D3D12_CPU_DESCRIPTOR_HANDLE cpuRtv = cpuBase;
				cpuRtv.ptr += static_cast<SIZE_T>(slot) * inc;
				device->CreateRenderTargetView(
					e.resource.Get(), nullptr, cpuRtv
				);

				if (fi == mCurrentFrameIndex) {
					e.rtvCpu = cpuRtv; // ビューポート用に保存
				}
			}
		}

		if (e.desc.allowDsv) {
			auto*      dsvHeap = mDx.GetDsvHeap();
			auto       cpuBase = dsvHeap->GetCPUDescriptorHandleForHeapStart();
			const UINT inc     = mDx.GetDsvDescriptorSize();

			D3D12_CPU_DESCRIPTOR_HANDLE cpuDsv = cpuBase;
			cpuDsv.ptr += static_cast<SIZE_T>(e.dsvLocal) * inc;

			D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
			dsv.Format                        = e.desc.dsvFormat.value();
			dsv.ViewDimension                 = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsv.Flags                         = D3D12_DSV_FLAG_NONE;

			device->CreateDepthStencilView(e.resource.Get(), &dsv, cpuDsv);

			e.dsvCpu = cpuDsv;
		}

		e.srvRevision = ++mGlobalSrvRevision;
	}
}
