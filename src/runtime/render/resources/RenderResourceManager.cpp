#include <engine/public/urenderer/renderresourcemanager/RenderResourceManager.h>

#include "runtime/assets/core/UAssetManager.h"
#include "runtime/assets/types/TextureAsset.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3dx12.h>

#include <engine/public/subsystem/console/Log.h>
#include <engine/public/urenderer/GraphicsDevice.h>
#include <engine/public/utils/memory/MemUtil.h>
#include <engine/public/uuploadarena/UploadArena.h>

namespace Unnamed {
	using namespace Microsoft::WRL;

	constexpr std::string_view kChannel = "RenderResourceManager";

	namespace {
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT CalcFootprint2D(
			ID3D12Device*  device, const DXGI_FORMAT format,
			const uint32_t w, const uint32_t         h,
			const uint32_t mips = 1
		) {
			D3D12_RESOURCE_DESC desc = {};
			desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			desc.Format              = format;
			desc.Width               = w;
			desc.Height              = h;
			desc.DepthOrArraySize    = 1;
			desc.MipLevels           = static_cast<UINT16>(mips);
			desc.SampleDesc.Count    = 1;

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT footPrint = {};
			UINT                               numRows   = 0;
			UINT64                             rowSize   = 0;
			UINT64                             totalSize = 0;
			device->GetCopyableFootprints(
				&desc, 0, 1, 0, &footPrint, &numRows, &rowSize, &totalSize);
			return footPrint;
		}
	}

	RenderResourceManager::RenderResourceManager(
		GraphicsDevice* gd, UAssetManager* am, UploadArena* arena)
		: mGd(gd), mAssetManager(am), mArena(arena) {
	}

	TextureHandle RenderResourceManager::AcquireTexture(
		AssetID                    asset,
		ID3D12GraphicsCommandList* commandList
	) {
		std::scoped_lock lock(mMutex);

		// すでに実体化しているか確認
		if (auto it = mAssetToTexture.find(asset); it != mAssetToTexture.
			end()) {
			auto h = it->second;
			if (h.id < mTextures.size() && mTextures[h.id].alive && mTextures[h.
				id].gen == h.gen) {
				mTextures[h.id].refs++;
				return h;
			}
			// TODO: 古い/壊れている場合は作り直す
		}

		// 論理データ取得
		const auto* texAsset = mAssetManager->Get<TextureAssetData>(asset);
		if (!texAsset || texAsset->mips.empty()) {
			Warning(
				kChannel,
				"Failed to get texture asset data. AssetID={}",
				asset
			);
			return {};
		}

		// スロットの確保
		uint32_t index;
		if (!mFreeTextureList.empty()) {
			index = mFreeTextureList.back();
			mFreeTextureList.pop_back();
			UASSERT(index < mTextures.size());
		} else {
			index = static_cast<uint32_t>(mTextures.size());
			mTextures.emplace_back();
		}

		auto& texture = mTextures[index];
		texture.alive = true;
		texture.gen++;
		texture.refs        = 1;
		texture.sourceAsset = asset;


		// リソースの作成
		const uint32_t mipCount =
			!texAsset->mips.empty() ?
				static_cast<uint32_t>(texAsset->mips.size()) :
				1;
		const uint32_t w = texAsset->width;
		const uint32_t h = texAsset->height;

		D3D12_RESOURCE_DESC rd = {};
		rd.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		rd.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
		rd.Width               = w;
		rd.Height              = h;
		rd.DepthOrArraySize    = 1;
		rd.MipLevels           = static_cast<UINT16>(mipCount);
		rd.SampleDesc.Count    = 1;

		D3D12_HEAP_PROPERTIES hp  = {D3D12_HEAP_TYPE_DEFAULT};
		auto*                 dev = mGd->Device();
		if (
			FAILED(
				dev->CreateCommittedResource(
					&hp,
					D3D12_HEAP_FLAG_NONE,
					&rd,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&texture.resource)
				)
			)
		) {
			texture.alive = false;
			mFreeTextureList.emplace_back(index);
			return {};
		}

		// 転送
		if (texAsset->mips.empty()) {
			const size_t rowPitch = size_t(w) * 4;
			if (!UploadRGBA8_1Mip(texture, texAsset->bytes.data(), rowPitch, w,
			                      h, commandList)) {
				texture.alive = false;
				mFreeTextureList.emplace_back(index);
				return {};
			}
		} else {
			for (int m = static_cast<int>(mipCount) - 1; m >= 0; --m) {
				const auto& mip = texAsset->mips[m];

				D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp      = {};
				UINT                               numRows = 0;
				UINT64                             rb      = 0;
				UINT64                             total   = 0;
				dev->GetCopyableFootprints(
					&rd, static_cast<UINT>(m), 1, 0, &fp, &numRows, &rb, &total
				);

				constexpr uint64_t kAlign =
					D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
				auto slice = mArena->Allocate(
					static_cast<uint64_t>(fp.Footprint.RowPitch) * mip.height,
					kAlign);
				if (!slice.cpu) {
					// 読めなかったら保留してキューにぶち込む
					for (int k = m; k >= 0; --k) {
						const auto&      mip2 = texAsset->mips[k];
						PendingMipUpload p = {};
						p.tex = {index, texture.gen};
						p.mip = k;
						p.width = mip2.width;
						p.height = mip2.height;
						p.rowPitch = mip2.rowPitch;
						p.data = std::make_shared<std::vector<uint8_t>>(
							mip2.bytes);
						mDeferredMipUploads.emplace_back(std::move(p));
					}
					break;
				}

				// 行コピー
				auto dst = static_cast<uint8_t*>(slice.cpu);
				for (uint32_t y = 0; y < mip.height; ++y) {
					memcpy(
						dst + static_cast<size_t>(y) * fp.Footprint.RowPitch,
						mip.bytes.data() + static_cast<size_t>(y) * mip.
						rowPitch,
						std::min<size_t>(mip.rowPitch, fp.Footprint.RowPitch)
					);
				}

				// サブリソース毎に Copy -> ピクセルシェーダーリソースへ
				D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
				D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
				dstLoc.pResource = texture.resource.Get();
				dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				dstLoc.SubresourceIndex = static_cast<UINT>(m);

				srcLoc.pResource = mArena->Resource();
				srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
				srcLoc.PlacedFootprint = fp;
				srcLoc.PlacedFootprint.Offset = slice.offset;

				commandList->CopyTextureRegion(
					&dstLoc, 0, 0, 0, &srcLoc, nullptr
				);

				auto toPs = CD3DX12_RESOURCE_BARRIER::Transition(
					texture.resource.Get(),
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					static_cast<UINT>(m)
				);
				commandList->ResourceBarrier(1, &toPs);
			}

			// VRAMメモリを計算
			texture.vramBytes = 0;
			for (uint32_t m = 0; m < mipCount; ++m) {
				const uint32_t mw = std::max<uint32_t>(1u, w >> m);
				const uint32_t mh = std::max<uint32_t>(1u, h >> m);
				texture.vramBytes +=
					MemUtil::AlignUp(
						static_cast<size_t>(mw) * 4, 256
					) * mh;
			}
		}

		// SRVの作成
		auto* srvAlloc   = mGd->GetSrvAllocator();
		texture.srvIndex = srvAlloc->Allocate();
		if (texture.srvIndex == UINT32_MAX) {
			texture.resource.Reset();
			texture.alive = false;
			mFreeTextureList.emplace_back(index);
			return {};
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC sd = {};
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		sd.Texture2D.MipLevels = rd.MipLevels;
		dev->CreateShaderResourceView(
			texture.resource.Get(), &sd,
			srvAlloc->CPUHandle(texture.srvIndex)
		);

		TextureHandle handle   = {.id = index, .gen = texture.gen};
		mAssetToTexture[asset] = handle;
		return handle;
	}

	void RenderResourceManager::ProcessDeferredMipUploads(
		ID3D12GraphicsCommandList* commandList
	) {
		std::scoped_lock lock(mMutex);

		if (mDeferredMipUploads.empty()) {
			return;
		}

		auto* dev = mGd->Device();

		// 1フレームの上限
		const uint64_t budget = 32ull * 1024 * 1024;
		uint64_t       used   = 0;

		size_t i = 0;
		while (i < mDeferredMipUploads.size()) {
			auto& p = mDeferredMipUploads[i];
			if (p.tex.id >= mTextures.size()) {
				mDeferredMipUploads.erase(mDeferredMipUploads.begin() + i);
				continue;
			}
			auto& t = mTextures[p.tex.id];
			if (!t.alive || t.gen != p.tex.gen || !t.resource) {
				mDeferredMipUploads.erase(mDeferredMipUploads.begin() + i);
				continue;
			}

			auto                               rd      = t.resource->GetDesc();
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp      = {};
			UINT                               numRows = 0;
			UINT64                             rb      = 0;
			UINT64                             total   = 0;
			dev->GetCopyableFootprints(
				&rd, p.mip, 1, 0, &fp, &numRows, &rb, &total
			);

			// 予算チェック
			if (used + fp.Footprint.RowPitch * p.height > budget) {
				break;
			}

			// Arena確保
			constexpr uint64_t kAlign = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
			const auto         slice  = mArena->Allocate(
				static_cast<uint64_t>(fp.Footprint.RowPitch * p.height),
				kAlign
			);
			if (!slice.cpu) {
				break;
			}

			// 行コピー
			auto dst = static_cast<uint8_t*>(slice.cpu);
			for (uint32_t y = 0; y < p.height; ++y) {
				memcpy(
					dst + static_cast<size_t>(y) * fp.Footprint.RowPitch,
					p.data->data() + static_cast<size_t>(y) * p.rowPitch,
					std::min<size_t>(fp.Footprint.RowPitch, p.rowPitch)
				);
			}

			// 遷移してコピーして戻す
			auto toCopy = CD3DX12_RESOURCE_BARRIER::Transition(
				t.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_COPY_DEST, p.mip
			);
			commandList->ResourceBarrier(1, &toCopy);

			D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
			D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
			dstLoc.pResource = t.resource.Get();
			dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dstLoc.SubresourceIndex = p.mip;

			srcLoc.pResource = mArena->Resource();
			srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			srcLoc.PlacedFootprint = fp;
			srcLoc.PlacedFootprint.Offset = slice.offset;

			commandList->CopyTextureRegion(
				&dstLoc, 0, 0, 0, &srcLoc, nullptr
			);

			auto toPs = CD3DX12_RESOURCE_BARRIER::Transition(
				t.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, p.mip
			);
			commandList->ResourceBarrier(1, &toPs);

			used += fp.Footprint.RowPitch * p.height;

			mDeferredMipUploads.erase(mDeferredMipUploads.begin() + i);
		}
	}

	void RenderResourceManager::ReleaseTexture(
		const TextureHandle handle,
		ID3D12Fence*        fence,
		const uint64_t      value
	) {
		std::scoped_lock lock(mMutex);
		if (!handle.IsValid() || handle.id >= mTextures.size()) {
			return;
		}
		auto& texture = mTextures[handle.id];
		if (!texture.alive || texture.gen != handle.gen) {
			return;
		}

		if (texture.refs > 0) {
			texture.refs--;
		}
		if (texture.refs == 0) {
			texture.retireFence = fence;
			texture.retireValue = value;
			texture.alive       = false;
		}
	}

	D3D12_GPU_DESCRIPTOR_HANDLE RenderResourceManager::GetSrvGPU(
		const TextureHandle handle) const {
		std::scoped_lock                   lock(mMutex);
		static D3D12_GPU_DESCRIPTOR_HANDLE null = {};
		if (!handle.IsValid() || handle.id >= mTextures.size()) {
			return null;
		}
		const auto& texture = mTextures[handle.id];
		if (
			!texture.alive ||
			texture.gen != handle.gen ||
			texture.srvIndex == UINT32_MAX) {
			return null;
		}
		return mGd->GetSrvAllocator()->GPUHandle(texture.srvIndex);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RenderResourceManager::GetSrvCPU(
		const TextureHandle handle) const {
		std::scoped_lock                   lock(mMutex);
		static D3D12_CPU_DESCRIPTOR_HANDLE null = {};
		if (!handle.IsValid() || handle.id >= mTextures.size()) {
			return null;
		}
		const auto& texture = mTextures[handle.id];
		if (
			!texture.alive ||
			texture.gen != handle.gen ||
			texture.srvIndex == UINT32_MAX) {
			return null;
		}
		return mGd->GetSrvAllocator()->CPUHandle(texture.srvIndex);
	}

	UAssetManager* RenderResourceManager::GetAssetManager() const {
		return mAssetManager;
	}

	void RenderResourceManager::TrackUpload(
		const ComPtr<ID3D12Resource>& resource
	) {
		std::scoped_lock lock(mMutex);
		mUploadsThisFrame.emplace_back(resource);
	}

	void RenderResourceManager::FlushUploads(
		ID3D12Fence* fence, const uint64_t value
	) {
		std::scoped_lock lock(mMutex);
		if (!fence || mUploadsThisFrame.empty()) {
			return;
		}
		for (auto& u : mUploadsThisFrame) {
			PendingUpload p;
			p.resource = std::move(u);
			p.fence    = fence;
			p.value    = value;
			mPendingUploads.emplace_back(std::move(p));
		}
		mUploadsThisFrame.clear();
	}

	void RenderResourceManager::GarbageCollect() {
		std::scoped_lock lock(mMutex);
		auto*            srvAlloc = mGd->GetSrvAllocator();

		for (auto& tex : mTextures) {
			if (
				!tex.alive &&
				tex.retireFence &&
				tex.retireFence->GetCompletedValue() >= tex.retireValue
			) {
				if (tex.srvIndex != UINT32_MAX) {
					srvAlloc->Free(tex.srvIndex);
					tex.srvIndex = UINT32_MAX;
				}
				tex.resource.Reset();

				tex.retireFence.Reset();
				tex.retireValue = 0;
				tex.vramBytes   = 0;
			}

			// 参照カウントが0で、フェンスも0なら空きにする
			if (!tex.alive && tex.refs == 0 && !tex.retireFence) {
				if (tex.sourceAsset != kInvalidAssetID) {
					auto it = mAssetToTexture.find(tex.sourceAsset);
					if (
						it != mAssetToTexture.end() &&
						it->second.id == (&tex - mTextures.data())) {
						mAssetToTexture.erase(it);
					}
				}
				mFreeTextureList.emplace_back(
					static_cast<uint32_t>(&tex - mTextures.data()));
			}
		}

		// アップロード待ちの削除
		auto it = mPendingUploads.begin();
		while (it != mPendingUploads.end()) {
			uint64_t completed = it->fence ? it->fence->GetCompletedValue() : 0;
			if (completed >= it->value) {
				it = mPendingUploads.erase(it);
				DevMsg(
					kChannel,
					"Garbage collected an upload buffer"
				);
			} else {
				++it;
			}
		}
	}

	uint32_t RenderResourceManager::GpuRefCount(TextureHandle handle) const {
		std::scoped_lock lock(mMutex);
		if (!handle.IsValid() || handle.id >= mTextures.size()) {
			return 0;
		}
		const auto& tex = mTextures[handle.id];
		if (!tex.alive || tex.gen != handle.gen) {
			return 0;
		}
		return tex.refs;
	}

	size_t RenderResourceManager::VramUsageBytes() const {
		std::scoped_lock lock(mMutex);
		size_t           total = 0;
		for (auto& tex : mTextures) {
			if (tex.alive) {
				total += tex.vramBytes;
			}
		}
		return total;
	}

	bool RenderResourceManager::CreateStaticVertexBuffer(
		const void* data, const size_t bytes, const UINT stride, GpuVB& out
	) const {
		out.handle = mGd->CreateVertexBuffer(data, bytes);
		out.stride = stride;
		return out.handle.id != 0 || bytes == 0;
	}

	bool RenderResourceManager::CreateStaticIndexBuffer(
		const void* data, const size_t bytes, const DXGI_FORMAT fmt, GpuIB& out
	) const {
		out.handle = mGd->CreateIndexBuffer(data, bytes, fmt);
		out.format = fmt;
		return out.handle.id != 0 || bytes == 0;
	}

	void RenderResourceManager::BindVertexBuffer(
		ID3D12GraphicsCommandList* cmd, const GpuVB& vb
	) const {
		mGd->BindVertexBuffer(cmd, vb.handle, vb.stride, 0);
	}

	void RenderResourceManager::BindIndexBuffer(
		ID3D12GraphicsCommandList* cmd, const GpuIB& ib
	) const {
		mGd->BindIndexBuffer(cmd, ib.handle, ib.format, 0);
	}

	bool RenderResourceManager::UploadRGBA8_1Mip(
		GpuTexture&    texture,
		const void*    pixels,
		const size_t   rowPitchBytes,
		const uint32_t w, const uint32_t h,
		ID3D12GraphicsCommandList*
		commandList
	) const {
		// デフォルトテクスチャ作成
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Width               = w;
		desc.Height              = h;
		desc.DepthOrArraySize    = 1;
		desc.MipLevels           = 1;
		desc.SampleDesc.Count    = 1;

		D3D12_HEAP_PROPERTIES hp  = {D3D12_HEAP_TYPE_DEFAULT};
		auto*                 dev = mGd->Device();
		if (
			FAILED(
				dev->CreateCommittedResource(
					&hp,
					D3D12_HEAP_FLAG_NONE,
					&desc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&texture.resource)
				)
			)
		) {
			return false;
		}

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp;
		UINT                               numRows;
		UINT64                             rowByte;
		UINT64                             totalBytes;
		dev->GetCopyableFootprints(
			&desc, 0, 1, 0,
			&fp, &numRows, &rowByte, &totalBytes
		);

		// UploadArena から確保
		constexpr uint64_t kTexPlaceAlign =
			D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
		const auto slice = mArena->
			Allocate(static_cast<uint64_t>(fp.Footprint.RowPitch) * h,
			         kTexPlaceAlign);
		if (!slice.cpu) {
			return false;
		}

		// マップと行コピー (RowPitch差を吸収)
		const auto src = static_cast<const uint8_t*>(pixels);
		const auto dst = static_cast<uint8_t*>(slice.cpu);
		for (uint32_t y = 0; y < h; ++y) {
			memcpy(
				dst + size_t(y) * fp.Footprint.RowPitch,
				src - +size_t(y) * rowPitchBytes,
				std::min<size_t>(rowPitchBytes, fp.Footprint.RowPitch)
			);
		}

		// TextureRegionのコピー
		D3D12_TEXTURE_COPY_LOCATION dstLoc;
		D3D12_TEXTURE_COPY_LOCATION srcLoc;
		dstLoc.pResource        = texture.resource.Get();
		dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstLoc.SubresourceIndex = 0;

		srcLoc.pResource = mArena->Resource();
		srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		srcLoc.PlacedFootprint = fp;
		srcLoc.PlacedFootprint.Offset = slice.offset;

		commandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			texture.resource.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);
		commandList->ResourceBarrier(1, &barrier);

		texture.vramBytes = static_cast<size_t>(fp.Footprint.RowPitch) * h;
		texture.w         = w;
		texture.h         = h;
		texture.format    = desc.Format;

		return true;
	}
}
