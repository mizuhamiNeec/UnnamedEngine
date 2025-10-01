#include <pch.h>

//-----------------------------------------------------------------------------

#include <engine/subsystem/console/Log.h>
#include <engine/urenderer/GraphicsDevice.h>
#include <runtime/core/Properties.h>

//-----------------------------------------------------------------------------

#include <d3dcompiler.h>
#include <d3dx12.h>
#ifndef DFCC_DXIL
#define DFCC_DXIL MAKEFOURCC('D', 'X', 'I', 'L')
#endif
#include <dxcapi.h>

namespace Unnamed {
	using namespace Microsoft::WRL;

	constexpr std::string_view kChannel = "GraphicsDevice";

	namespace {
		template <typename T, typename U>
		ComPtr<T> QueryInterface(const ComPtr<U>& base) {
			ComPtr<T> result;
			if (base && SUCCEEDED(base.As(&result))) {
				return result;
			}
			return nullptr;
		}

		// 仮実装
		ComPtr<IDxcBlob> CompileShaderFromString(
			const std::string& source,
			const std::string& entryPoint,
			const std::string& profile
		) {
			ComPtr<IDxcUtils>          dxcUtils;
			ComPtr<IDxcCompiler3>      dxcCompiler;
			ComPtr<IDxcIncludeHandler> includeHandler;
			HRESULT                    hr = DxcCreateInstance(CLSID_DxcUtils,
			                               IID_PPV_ARGS(&dxcUtils));
			if (FAILED(hr)) {
				Warning(kChannel, "Failed to create DxcUtils.");
				return nullptr;
			}
			hr = DxcCreateInstance(CLSID_DxcCompiler,
			                       IID_PPV_ARGS(&dxcCompiler));
			if (FAILED(hr)) {
				Warning(kChannel, "Failed to create DxcCompiler.");
				return nullptr;
			}
			hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
			if (FAILED(hr)) {
				Warning(kChannel, "Failed to create include handler.");
				return nullptr;
			}


			ComPtr<IDxcBlobEncoding> sourceBlob;
			hr = dxcUtils->CreateBlobFromPinned(
				source.c_str(),
				static_cast<UINT32>(source.size()),
				CP_UTF8,
				&sourceBlob
			);
			if (FAILED(hr)) {
				Warning(kChannel, "Failed to create source blob.");
				return nullptr;
			}

			DxcBuffer shaderSourceBuffer;
			shaderSourceBuffer.Ptr      = sourceBlob->GetBufferPointer();
			shaderSourceBuffer.Size     = sourceBlob->GetBufferSize();
			shaderSourceBuffer.Encoding = DXC_CP_UTF8;

			const std::wstring wEntryPoint = StrUtil::ToWString(entryPoint);
			const std::wstring wProfile    = StrUtil::ToWString(profile);

			LPCWSTR arguments[] = {
				L"-E", wEntryPoint.c_str(),      // エントリーポイントの指定。
				L"-T", wProfile.c_str(),         // ShaderProfileの設定
				DXC_ARG_DEBUG, L"-Qembed_debug", // デバッグ用の情報を埋め込む
				DXC_ARG_SKIP_OPTIMIZATIONS,      // 最適化を外しておく
				DXC_ARG_PACK_MATRIX_ROW_MAJOR,   // 行列のメモリレイアウトは行優先
			};

			ComPtr<IDxcResult> shaderResult;
			hr = dxcCompiler->Compile(
				&shaderSourceBuffer,        // 読み込んだファイル
				arguments,                  // コンパイルオプション
				_countof(arguments),        // コンパイルオプションの数
				includeHandler.Get(),       // includeが含まれた諸々
				IID_PPV_ARGS(&shaderResult) // コンパイル結果
			);

			if (FAILED(hr)) {
				Fatal(kChannel, "Failed to launch DXC.");
				UASSERT(SUCCEEDED(hr));
			}

			ComPtr<IDxcBlobUtf8> shaderError;
			shaderResult->GetOutput(
				DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr
			);
			if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
				Fatal(
					kChannel,
					"{}",
					shaderError->GetStringPointer()
				);
				UASSERT(false);
			}

			ComPtr<IDxcBlob> shaderBlob;
			hr = shaderResult->GetOutput(
				DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr
			);
			UASSERT(SUCCEEDED(hr));
			Msg(
				kChannel,
				"Shader compile success: entryPoint='{}', profile='{}'",
				entryPoint, profile
			);

			return shaderBlob;
		}
	}

	bool GraphicsDevice::Init(GraphicsDeviceInfo info) {
		mInfo = info;

		// Enable Debug Layer
		if (info.bEnableDebug) {
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(
				D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
				Msg(kChannel, "D3D12 debug layer enabled.");
			} else {
				Warning(kChannel, "D3D12 debug layer is not available.");
			}

			if (const auto debug1 = QueryInterface<ID3D12Debug1>(
				debugController)) {
				debug1->SetEnableGPUBasedValidation(true);
				Msg(kChannel, "D3D12 GPU-based validation enabled.");
			} else {
				Warning(
					kChannel, "D3D12 GPU-based validation is not available."
				);
			}

			if (const auto debug3 = QueryInterface<ID3D12Debug3>(
				debugController)) {
				debug3->SetEnableSynchronizedCommandQueueValidation(true);
				Msg(
					kChannel,
					"D3D12 synchronized command queue validation enabled."
				);
			} else {
				Warning(
					kChannel,
					"D3D12 synchronized command queue validation is not available."
				);
			}

			if (const auto debug5 = QueryInterface<ID3D12Debug5>(
				debugController)) {
				debug5->SetEnableAutoName(true); // 自動で名前をつけてくれるらしい...まじすか!?
				Msg(kChannel, "D3D12 automatic naming enabled.");
			} else {
				Warning(kChannel, "D3D12 automatic naming is not available.");
			}

			SpecialMsg(
				LogLevel::Success,
				kChannel,
				"Debug Layer enabled."
			);
		}

		if (info.bEnableDebug) {
			ComPtr<ID3D12Debug> debug;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
				debug->EnableDebugLayer();
			} else {
				Warning(
					"GraphicsDevice", "Debug layer is not available."
				);
			}
		}

		ComPtr<IDXGIFactory6> factory;
		THROW(
			CreateDXGIFactory2(
				info.bEnableDebug ? DXGI_CREATE_FACTORY_DEBUG : 0,
				IID_PPV_ARGS(&factory)
			)
		);

		// デバイスの作成
		{
			ComPtr<IDXGIAdapter4> adapter;

			UINT flags = 0;

#ifdef _DEBUG
			flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

			THROW(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory)));

			constexpr std::array gpuPreferences = {
				DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
				DXGI_GPU_PREFERENCE_MINIMUM_POWER,
				DXGI_GPU_PREFERENCE_UNSPECIFIED,
			};

			if (const auto factory6 = QueryInterface<IDXGIFactory6>(factory)) {
				bool found = false;
				for (const auto preference : gpuPreferences) {
					UINT i = 0;
					while (
						factory6->EnumAdapterByGpuPreference(
							i, preference, IID_PPV_ARGS(&adapter)
						) != DXGI_ERROR_NOT_FOUND) {
						DXGI_ADAPTER_DESC3 adapterDesc;
						THROW(adapter->GetDesc3(&adapterDesc));
						Msg(
							kChannel,
							"Found Adapter : {}",
							StrUtil::ToString(adapterDesc.Description)
						);
						if (
							!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)
						) {
							SpecialMsg(
								LogLevel::None,
								kChannel,
								"    Use Adapter : {}",
								StrUtil::ToString(adapterDesc.Description)
							);
							SpecialMsg(
								LogLevel::None,
								kChannel,
								"    GPU Details: Vendor ID: 0x{:X}, Device ID: 0x{:X}",
								adapterDesc.VendorId, adapterDesc.DeviceId
							);
							SpecialMsg(
								LogLevel::None,
								kChannel,
								"    GPU Memory: Dedicated Video: {:.2f} MB, Dedicated System: {:.2f} MB, Shared System: {:.2f} MB",
								static_cast<double>(
									adapterDesc.DedicatedVideoMemory
								) / (1024.0 * 1024.0),
								static_cast<double>(
									adapterDesc.DedicatedSystemMemory
								) / (1024.0 * 1024.0),
								static_cast<double>(
									adapterDesc.SharedSystemMemory
								) / (1024.0 * 1024.0)
							);
							SpecialMsg(
								LogLevel::None,
								kChannelNone,
								"    GPU Revision: {}, SubSys ID: 0x{:X}",
								adapterDesc.Revision, adapterDesc.SubSysId
							);
							found = true;
							break;
						}
						adapter.Reset();
						++i;
					}
					if (found) break;
				}
				UASSERT(adapter != nullptr && "No suitable adapter found.");
			} else {
				Msg(
					kChannel,
					"それはちょっとねぇ、世間は許してくrえゃすぇんよ"
				);
				UASSERT(false && "IDXGIFactory6 is not supported.");
			}

			if (const auto factory7 = QueryInterface<IDXGIFactory7>(factory)) {
				mAdapterChangeEvent = CreateEvent(
					nullptr, FALSE, FALSE, nullptr);
				if (!mAdapterChangeEvent) {
					Msg(
						kChannel,
						"CreateEvent failed."
					);
					return false;
				}
				HRESULT hr = factory7->RegisterAdaptersChangedEvent(
					mAdapterChangeEvent, &mAdapterChangeEventCookie
				);
				if (FAILED(hr)) {
					Msg(
						kChannel,
						"RegisterAdaptersChangedEvent failed. HRESULT: {}",
						std::to_string(hr)
					);
					return false;
				}
				Msg(
					kChannel,
					"Registered for adapter changes."
				);
			}

			constexpr std::array featureLevels = {
				D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1,
				D3D_FEATURE_LEVEL_12_0,
				D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0
			};

			std::array featureLevelStrings = {
				"12.2", "12.1", "12.0", "11.1", "11.0"
			};

			for (size_t i = 0; i < featureLevels.size(); ++i) {
				if (
					SUCCEEDED(
						D3D12CreateDevice(
							adapter.Get(), featureLevels[i],
							IID_PPV_ARGS(&mDevice)
						)
					)
				) {
					Msg(
						kChannel,
						"Created D3D12 device with feature level {}",
						featureLevelStrings[i]
					);
					break;
				}
				mDevice.Reset();
			}
			UASSERT(mDevice != nullptr && "Failed to create D3D12 device.");
		}

		// 検証レイヤーの有効化
		{
			if (info.bEnableDebug) {
				ComPtr<ID3D12InfoQueue> infoQueue;
				if (
					SUCCEEDED(mDevice->QueryInterface(IID_PPV_ARGS(&infoQueue)))
				) {
					// やばいエラー時に止まる
					infoQueue->SetBreakOnSeverity(
						D3D12_MESSAGE_SEVERITY_CORRUPTION, true
					);
					// エラー時に止まる
					infoQueue->SetBreakOnSeverity(
						D3D12_MESSAGE_SEVERITY_ERROR, true
					);
					// 警告時に止まる
					infoQueue->SetBreakOnSeverity(
						D3D12_MESSAGE_SEVERITY_WARNING, true
					);

					// 抑制するメッセージのID
					std::array denyIds = {
						// Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
						// https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
						D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
						// TODO: 流石に改善されたかな?
						D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED
						// TODO: これなんだ?
					};

					// 抑制するレベル
					std::array severities = {D3D12_MESSAGE_SEVERITY_INFO};
					D3D12_INFO_QUEUE_FILTER filter = {};
					filter.DenyList.NumIDs = static_cast<UINT>(denyIds.size());
					filter.DenyList.pIDList = denyIds.data();
					filter.DenyList.NumSeverities = static_cast<UINT>(severities
						.size());
					filter.DenyList.pSeverityList = severities.data();
					// 指定したメッセージの表示を抑制する
					THROW(infoQueue->PushStorageFilter(&filter));
				}
			}
		}

		// コマンドキューの作成
		{
			D3D12_COMMAND_QUEUE_DESC queueDesc = {
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				0,
				D3D12_COMMAND_QUEUE_FLAG_NONE,
				0
			};

			THROW(
				mDevice->CreateCommandQueue(
					&queueDesc, IID_PPV_ARGS(&mCommandQueue)
				)
			);

			THROW(mCommandQueue->SetName(L"UnnamedRendererCommandQueue"));

			Msg(
				kChannel,
				"Created command queue for direct command list type."
			);
		}

		// 各種ディスクリプタアロケータの初期化
		{
			mSrvAllocator     = std::make_unique<DescriptorAllocator>();
			mSamplerAllocator = std::make_unique<DescriptorAllocator>();
			mRtvAllocator     = std::make_unique<DescriptorAllocator>();
			mDsvAllocator     = std::make_unique<DescriptorAllocator>();

			mSrvAllocator->Init(
				mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				kSrvHeapCount, true
			);
			mSamplerAllocator->Init(
				mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
				kSamplerHeapCount, true
			);
			mRtvAllocator->Init(
				mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
				kRtvHeapCount
			);
			mDsvAllocator->Init(
				mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
				kDsvHeapCount
			);
		}

		// スワップチェーンの作成
		{
			if (const auto factory7 = QueryInterface<IDXGIFactory7>(factory)) {
				DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
				swapChainDesc.Width = info.width;
				swapChainDesc.Height = info.height;
				swapChainDesc.Format = kBufferFormat;
				swapChainDesc.BufferCount = kFrameBufferCount;
				swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
				swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
				swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない

				ComPtr<IDXGISwapChain1> tmp;
				THROW(
					factory7->CreateSwapChainForHwnd(
						mCommandQueue.Get(),
						static_cast<HWND>(info.hWnd),
						&swapChainDesc,
						nullptr,
						nullptr,
						&tmp
					)
				);

				THROW(tmp.As(&mSwapChain));
				mBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();
			}
		}

		// RTVの作成
		{
			D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
			rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvDesc.NumDescriptors = kFrameBufferCount;
			rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			for (UINT i = 0; i < kFrameBufferCount; ++i) {
				THROW(
					mSwapChain->GetBuffer(
						i, IID_PPV_ARGS(&mBackBuffers[i]. resource)
					)
				);
				auto index          = mRtvAllocator->Allocate();
				mBackBuffers[i].rtv = mRtvAllocator->CPUHandle(index);
				mDevice->CreateRenderTargetView(mBackBuffers[i].resource.Get(),
				                                nullptr, mBackBuffers[i].rtv);
			}

			for (UINT i = 0; i < kFrameBufferCount; ++i) {
				THROW(
					mDevice->CreateCommandAllocator(
						D3D12_COMMAND_LIST_TYPE_DIRECT,
						IID_PPV_ARGS(&mFrameContexts[i].commandAllocator)
					)
				);

				THROW(
					mDevice->CreateCommandList(
						0,
						D3D12_COMMAND_LIST_TYPE_DIRECT,
						mFrameContexts[i].commandAllocator.Get(),
						nullptr,
						IID_PPV_ARGS(&mFrameContexts[i].commandList)
					)
				);

				THROW(mFrameContexts[i].commandList->Close());

				THROW(
					mDevice->CreateFence(
						0,
						D3D12_FENCE_FLAG_NONE,
						IID_PPV_ARGS(&mFrameContexts[i].fence)
					)
				);

				mFrameContexts[i].event = CreateEvent(
					nullptr, FALSE, FALSE, nullptr
				);
			}
		}

		// DSVの作成
		{
			CreateDepthBuffers(info.width, info.height);
		}

		return true;
	}

	void GraphicsDevice::Shutdown() {
		for (auto& frameContext : mFrameContexts) {
			if (frameContext.event) {
				CloseHandle(frameContext.event);
			}
		}
		mBuffers.clear();
	}

	FrameContext GraphicsDevice::BeginFrame(const Vec4 clearColor) const {
		auto& frameContext = mFrameContexts[mBackBufferIndex];
		WaitGPU(mBackBufferIndex);

		THROW(frameContext.commandAllocator->Reset());
		THROW(
			frameContext.commandList->Reset(
				frameContext.commandAllocator.Get(),
				nullptr
			)
		);

		D3D12_RESOURCE_BARRIER barrier;
		barrier.Type                 = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags                = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = mBackBuffers[mBackBufferIndex].resource.
			Get();
		barrier.Transition.Subresource =
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
		frameContext.commandList->ResourceBarrier(1, &barrier);

		auto& bb = mBackBuffers[mBackBufferIndex];
		auto& db = mDepthBuffers[mBackBufferIndex];

		frameContext.commandList->OMSetRenderTargets(
			1, &bb.rtv, TRUE, &db.dsv
		);
		const FLOAT color[4] = {
			clearColor.x, clearColor.y, clearColor.z, clearColor.w
		};
		color;
		frameContext.commandList->ClearRenderTargetView(
			bb.rtv, color, 0, nullptr
		);
		frameContext.commandList->ClearDepthStencilView(
			db.dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr
		);

		// ビューポート、シザーの設定
		D3D12_VIEWPORT viewport;
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width    = static_cast<FLOAT>(mInfo.width);
		viewport.Height   = static_cast<FLOAT>(mInfo.height);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		frameContext.commandList->RSSetViewports(1, &viewport);

		D3D12_RECT scissorRect;
		scissorRect.left   = 0;
		scissorRect.top    = 0;
		scissorRect.right  = static_cast<LONG>(mInfo.width);
		scissorRect.bottom = static_cast<LONG>(mInfo.height);
		frameContext.commandList->RSSetScissorRects(1, &scissorRect);

		return {
			.cmd = frameContext.commandList.Get(),
			.alloc = frameContext.commandAllocator.Get(),
			.backIndex = mBackBufferIndex
		};
	}

	void GraphicsDevice::EndFrame(const FrameContext& ctx) {
		D3D12_RESOURCE_BARRIER barrier;
		barrier.Type                 = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags                = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = mBackBuffers[ctx.backIndex].resource.
			Get();
		barrier.Transition.Subresource =
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
		ctx.cmd->ResourceBarrier(1, &barrier);

		THROW(ctx.cmd->Close());
		ID3D12CommandList* commandLists[] = {ctx.cmd};
		mCommandQueue->ExecuteCommandLists(
			_countof(commandLists), commandLists
		);

		const HRESULT hr = mSwapChain->Present(1, 0);
		if (FAILED(hr)) {
			Error(
				kChannel,
				"Failed to present swap chain. HRESULT: 0x{:X}",
				static_cast<unsigned long>(hr)
			);
		}

		auto& frame = mFrameContexts[ctx.backIndex];
		frame.fenceValue++;
		THROW(mCommandQueue->Signal(frame.fence.Get(), frame.fenceValue));

		mBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();
	}

	GraphicsDevice::PerFrame GraphicsDevice::GetFrameBuffer(
		const uint32_t frameIndex
	) const {
		return mFrameContexts[frameIndex];
	}

	/// 即時コンテキストを取得する。呼び出しごとにコマンドアロケータとコマンドリストがリセットされる。
	FrameContext GraphicsDevice::BeginImmediateFrame() const {
		WaitGPU(mBackBufferIndex);
		auto& frameContext = mFrameContexts[mBackBufferIndex];
		THROW(frameContext.commandAllocator->Reset());
		THROW(
			frameContext.commandList->Reset(
				frameContext.commandAllocator.Get(), nullptr
			)
		);

		const std::array heaps = {
			GetSrvAllocator()->GetHeap(), GetSamplerAllocator()->GetHeap()
		};
		frameContext.commandList->SetDescriptorHeaps(
			static_cast<UINT>(heaps.size()), heaps.data()
		);

		return {
			.cmd = frameContext.commandList.Get(),
			.alloc = frameContext.commandAllocator.Get(),
			.backIndex = mBackBufferIndex
		};
	}

	BufferHandle GraphicsDevice::CreateVertexBuffer(
		const void*  data,
		const size_t bytes
	) {
		return CreateStaticBuffer(
			data, bytes, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
		);
	}

	BufferHandle GraphicsDevice::CreateIndexBuffer(
		const void*  data,
		const size_t bytes,
		DXGI_FORMAT) {
		return CreateStaticBuffer(
			data, bytes, D3D12_RESOURCE_STATE_INDEX_BUFFER
		);
	}

	void GraphicsDevice::BindVertexBuffer(
		ID3D12GraphicsCommandList* commandList,
		const BufferHandle         bufferHandle,
		const UINT                 stride,
		const UINT                 offset
	) const {
		const auto&              [resource, size] = mBuffers[bufferHandle.id];
		D3D12_VERTEX_BUFFER_VIEW view;
		view.BufferLocation = resource->GetGPUVirtualAddress() + offset;
		view.SizeInBytes    = static_cast<UINT>(size - offset);
		view.StrideInBytes  = stride;
		commandList->IASetVertexBuffers(
			0, 1, &view
		);
	}

	void GraphicsDevice::BindIndexBuffer(
		ID3D12GraphicsCommandList* commandList,
		const BufferHandle         bufferHandle,
		const DXGI_FORMAT          format,
		const UINT                 offset
	) const {
		const auto&             [resource, size] = mBuffers[bufferHandle.id];
		D3D12_INDEX_BUFFER_VIEW view;
		view.BufferLocation = resource->GetGPUVirtualAddress() + offset;
		view.SizeInBytes    = static_cast<UINT>(size - offset);
		view.Format         = format;
		commandList->IASetIndexBuffer(&view);
	}

	void GraphicsDevice::DrawIndexed(
		ID3D12GraphicsCommandList* commandList,
		const UINT                 indexCount,
		const UINT                 start,
		const INT                  baseVertexLocation
	) {
		commandList->DrawIndexedInstanced(
			indexCount, 1, start, baseVertexLocation, 0
		);
	}

	ID3D12Device* GraphicsDevice::Device() const noexcept {
		return mDevice.Get();
	}

	IDXGISwapChain4* GraphicsDevice::SwapChain() const noexcept {
		return mSwapChain.Get();
	}

	DescriptorAllocator* GraphicsDevice::GetSrvAllocator() const {
		return mSrvAllocator.get();
	}

	DescriptorAllocator* GraphicsDevice::GetSamplerAllocator() const {
		return mSamplerAllocator.get();
	}

	DescriptorAllocator* GraphicsDevice::GetRtvAllocator() const {
		return mRtvAllocator.get();
	}

	DescriptorAllocator* GraphicsDevice::GetDsvAllocator() const {
		return mDsvAllocator.get();
	}

	static D3D12_PLACED_SUBRESOURCE_FOOTPRINT LayoutForTex2D(
		ID3D12Device* device,
		DXGI_FORMAT   format,
		uint32_t      w, uint32_t h,
		uint32_t      mipCount
	) {
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Format              = format;
		desc.Width               = w;
		desc.Height              = h;
		desc.DepthOrArraySize    = 1;
		desc.MipLevels           = static_cast<UINT16>(mipCount);
		desc.SampleDesc.Count    = 1;

		UINT64                             totalSize = 0;
		UINT                               numRows   = 0;
		UINT64                             rowSize   = 0;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footPrint = {};

		device->GetCopyableFootprints(
			&desc,
			0,
			1,
			0,
			&footPrint,
			&numRows,
			&rowSize,
			&totalSize
		);

		return footPrint;
	}

	void GraphicsDevice::WaitGPU(const uint32_t frameIndex) const {
		auto& frameContext = mFrameContexts[frameIndex];
		if (frameContext.fence->GetCompletedValue() < frameContext.fenceValue) {
			THROW(
				frameContext.fence->SetEventOnCompletion(
					frameContext.fenceValue, frameContext.event
				)
			);
			WaitForSingleObject(frameContext.event, INFINITE);
		}
	}

	BufferHandle GraphicsDevice::CreateStaticBuffer(
		const void*                 data,
		const size_t                bytes,
		const D3D12_RESOURCE_STATES initState
	) {
		BufferHandle handle = {static_cast<uint32_t>(mBuffers.size())};
		mBuffers.emplace_back();
		auto& buffer = mBuffers.back();
		buffer.size  = bytes;

		D3D12_HEAP_PROPERTIES heap = {};
		heap.Type                  = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width               = bytes;
		resourceDesc.Height              = 1;
		resourceDesc.DepthOrArraySize    = 1;
		resourceDesc.MipLevels           = 1;
		resourceDesc.Format              = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count    = 1;
		resourceDesc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags               = D3D12_RESOURCE_FLAG_NONE;

		THROW(
			mDevice->CreateCommittedResource(
				&heap, D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&buffer.resource)
			)
		);

		D3D12_HEAP_PROPERTIES upHeap = {};
		upHeap.Type                  = D3D12_HEAP_TYPE_UPLOAD;

		ComPtr<ID3D12Resource> upload;
		THROW(
			mDevice->CreateCommittedResource(
				&upHeap, D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&upload)
			)
		);

		void*                 mappedData = nullptr;
		constexpr D3D12_RANGE range      = {0, 0};
		THROW(upload->Map(0, &range, &mappedData));
		memcpy(mappedData, data, bytes);
		upload->Unmap(0, nullptr);

		ComPtr<ID3D12CommandAllocator> tempAllocator;
		THROW(mDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAllocator)));

		ComPtr<ID3D12GraphicsCommandList> tempCmdList;
		THROW(mDevice->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAllocator.Get(), nullptr,
			IID_PPV_ARGS(&tempCmdList)));

		tempCmdList->CopyBufferRegion(
			buffer.resource.Get(),
			0,
			upload.Get(),
			0,
			bytes
		);

		D3D12_RESOURCE_BARRIER barrier;
		barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource   = buffer.resource.Get();
		barrier.Transition.Subresource =
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter  = initState;
		tempCmdList->ResourceBarrier(1, &barrier);

		THROW(tempCmdList->Close());

		ID3D12CommandList* lists[] = {tempCmdList.Get()};
		mCommandQueue->ExecuteCommandLists(1, lists);

		// フェンスを使ってGPUを待つ
		ComPtr<ID3D12Fence> fence;
		THROW(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence
		)));
		HANDLE evt = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		UASSERT(evt && "Failed to create event for fence.");
		constexpr UINT64 fenceValue = 1;
		THROW(mCommandQueue->Signal(fence.Get(), fenceValue));
		THROW(fence->SetEventOnCompletion(fenceValue, evt));
		WaitForSingleObject(evt, INFINITE);
		CloseHandle(evt);

		return handle;
	}

	void GraphicsDevice::CreateDepthBuffers(UINT width, UINT height) {
		DestroyDepthBuffers();

		// DSV ディスクリプタ生成
		for (UINT i = 0; i < kFrameBufferCount; ++i) {
			D3D12_HEAP_PROPERTIES heap = {};
			heap.Type                  = D3D12_HEAP_TYPE_DEFAULT;

			D3D12_RESOURCE_DESC desc = {};
			desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			desc.Alignment           = 0;
			desc.Width               = width;
			desc.Height              = height;
			desc.DepthOrArraySize    = 1;
			desc.MipLevels           = 1;
			desc.Format              = kDepthFormat;
			desc.SampleDesc.Count    = 1;
			desc.SampleDesc.Quality  = 0;
			desc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			desc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			D3D12_CLEAR_VALUE clearValue    = {};
			clearValue.Format               = kDepthFormat;
			clearValue.DepthStencil.Depth   = 1.0f;
			clearValue.DepthStencil.Stencil = 0;

			THROW(
				mDevice->CreateCommittedResource(
					&heap, D3D12_HEAP_FLAG_NONE,
					&desc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
					&clearValue,
					IID_PPV_ARGS(&mDepthBuffers[i].resource )
				)
			);

			// DSV
			auto dsvIndex        = mDsvAllocator->Allocate();
			mDepthBuffers[i].dsv = mDsvAllocator->CPUHandle(dsvIndex);

			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = kDepthFormat;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
			mDevice->CreateDepthStencilView(
				mDepthBuffers[i].resource.Get(),
				&dsvDesc, mDepthBuffers[i].dsv
			);
		}
	}

	void GraphicsDevice::DestroyDepthBuffers() {
		for (auto& buffer : mDepthBuffers) {
			buffer.resource.Reset();
			buffer.dsv = {};
		}
	}
}
