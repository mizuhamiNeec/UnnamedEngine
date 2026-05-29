#include "D3D12Device.h"

#include <chrono>
#include <dxgidebug.h>

#include "D3D12SwapChain.h"
#include "D3D12Util.h"

#include "core/UnnamedMacro.h"
#include "core/string/StrUtil.h"

#include "engine/rhi/DxcShaderCompiler.h"
#include "engine/unnamed/subsystem/console/Log.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

namespace Unnamed::Rhi {
	static constexpr std::string_view kChannel = "D3D12";

	using namespace Microsoft::WRL;

	namespace {
		template <typename T, typename U>
		ComPtr<T> QueryInterface(const ComPtr<U>& source) {
			ComPtr<T>     result;
			const HRESULT hr = source.As(&result);
			UASSERT(SUCCEEDED(hr));
			return result;
		}
	}

	std::unique_ptr<IRhiDevice> CreateD3D12Device(
		void*                hwnd,
		const DeviceDesc&    deviceDesc,
		const SwapChainDesc& swapChainDesc
	) {
		// D3D12デバイスを作成して返す
		return std::make_unique<D3D12Device>(
			static_cast<HWND>(hwnd), deviceDesc, swapChainDesc
		);
	}

	D3D12Device::D3D12Device(
		HWND                 hwnd, const DeviceDesc& deviceDesc,
		const SwapChainDesc& swapChainDesc
	) : mHwnd(hwnd) {
		mFramesInFlight = swapChainDesc.bufferCount;
		mEnableVsync    = swapChainDesc.vSync;
		UASSERT(
			mFramesInFlight >= 2 && mFramesInFlight <= kMaxFramesInFlight &&
			"Invalid frame count."
		);

		if (!mDxcCompiler.Initialize()) {
			Error(kChannel, "DxcShaderCompiler の初期化に失敗しました");
			UASSERT(false);
		}

		if (deviceDesc.enableDebugLayer) {
			EnableDebugLayer(deviceDesc.enableGpuBasedValidation);
		}

		CreateDevice();

		if (deviceDesc.enableDebugLayer) {
			EnableValidationLayer();
		}

		CreateQueue();
		CreateSrvUavHeap();
		CreateRtvHeap();
		CreateDsvHeap();
		CreatePipelines();
		CreateCommandObjects();
		CreateFenceObjects();

		mSwapChain = std::make_unique<D3D12SwapChain>(
			mFactory,
			mDevice,
			mGraphicsQueue,
			hwnd,
			swapChainDesc
		);
	}

	D3D12Device::~D3D12Device() {
		// GPUの完了待ち
		WaitForGpuIdle();

		// リソースの解放
		mSwapChain.reset();
		mCommandList.Reset();
		for (auto& frame : mFrames) {
			frame.upload.Shutdown();
			frame.commandAllocator.Reset();
		}

		mSrvUavHeap.Reset();
		mRtvHeap.Reset();
		mDsvHeap.Reset();
		mFsRootSignature.Reset();
		mCsRootSignature.Reset();
		mGeomRootSignature.Reset();
		mGeomRootSignatureLinearClamp.Reset();
		mGeomRootSignaturePointClamp.Reset();

		mUploadContext.reset();

		// アダプタ変更イベントの解放
		if (mAdapterChangeEventCookie != 0) {
			if (mFactory7) {
				mFactory7->UnregisterAdaptersChangedEvent(
					mAdapterChangeEventCookie
				);
			}
			mAdapterChangeEventCookie = 0;
		}
		if (mAdapterChangeEvent) {
			CloseHandle(mAdapterChangeEvent);
			mAdapterChangeEvent = nullptr;
		}
		mFactory7.Reset();

		mGraphicsQueue.Reset();
		mFence.Reset();
		mDevice.Reset();
		mFactory.Reset();

		if (mFenceEvent) {
			CloseHandle(mFenceEvent);
			mFenceEvent = nullptr;
		}

#ifdef _DEBUG
		ComPtr<IDXGIDebug1> dxgiDebug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		}
#endif
	}

	void D3D12Device::BeginFrame() {
		const uint32_t frameIndex = mSwapChain->GetCurrentBackBufferIndex();

		WaitForFrame(frameIndex);

		// アロケータをリセットしてコマンドリストを開始する
		mFrames[frameIndex].upload.BeginFrame();
		Throw(mFrames[frameIndex].commandAllocator->Reset());
		Throw(
			mCommandList->Reset(
				mFrames[frameIndex].commandAllocator.Get(), nullptr
			)
		);
	}

	void D3D12Device::EndFrame() {
		const uint32_t frameIndex = mSwapChain->GetCurrentBackBufferIndex();

		// コマンドリストを閉じる
		Throw(mCommandList->Close());

		// コマンドリスト実行
		ID3D12CommandList* lists[] = {mCommandList.Get()};
		mGraphicsQueue->ExecuteCommandLists(1, lists);

		mSwapChain->Present(mEnableVsync);

		SignalFrame(frameIndex);
	}

	void D3D12Device::OnResize(const uint32_t width, const uint32_t height) {
		if (width == 0 || height == 0) {
			return;
		}

		const auto resizeStart = std::chrono::steady_clock::now();
		DevMsg(kChannel, "OnResize request: {}x{}", width, height);

		// GPUの完了待ち
		const auto waitStart = std::chrono::steady_clock::now();
		WaitForGpuIdle();
		const double waitMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - waitStart
		).count();

		mSwapChain->Resize(width, height);
		const double totalMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - resizeStart
		).count();
		DevMsg(
			kChannel,
			"OnResize complete: {}x{} (waitForGpuIdleMs={:.3f}, totalMs={:.3f})",
			width,
			height,
			waitMs,
			totalMs
		);
	}

	BACKEND_TYPE D3D12Device::GetBackendType() const {
		return BACKEND_TYPE::D3D12;
	}

	IRhiSwapChain& D3D12Device::GetSwapChain() {
		return *mSwapChain;
	}

	ID3D12GraphicsCommandList* D3D12Device::GetCommandList() const {
		return mCommandList.Get();
	}

	ID3D12CommandQueue* D3D12Device::GetGraphicsQueue() const {
		return mGraphicsQueue.Get();
	}

	D3D12SwapChain* D3D12Device::GetD3D12SwapChain() const {
		return mSwapChain.get();
	}

	ID3D12DescriptorHeap* D3D12Device::GetSrvUavHeap() const {
		return mSrvUavHeap.Get();
	}

	uint32_t D3D12Device::GetSrvUavDescriptorSize() const {
		return mSrvUavDescriptorSize;
	}

	uint32_t D3D12Device::GetSrvUavHeapCapacity() const {
		return mSrvUavHeapCapacity;
	}

	uint32_t D3D12Device::GetSrvUavTotalHeapCapacity() const {
		return mSrvUavTotalCapacity;
	}

	uint32_t D3D12Device::GetImGuiSrvHeapBase() const {
		return mImguiSrvHeapBase;
	}

	uint32_t D3D12Device::GetImGuiSrvHeapCapacity() const {
		return mImguiSrvHeapCapacity;
	}

	uint32_t D3D12Device::AllocateImGuiSrvSlot() {
		if (mImguiNextSlot >= mImguiSrvHeapCapacity) {
			Fatal(
				kChannel,
				"ImGui descriptor heap range exhausted: next={}, capacity={}",
				mImguiNextSlot,
				mImguiSrvHeapCapacity
			);
			return mImguiSrvHeapBase + (mImguiSrvHeapCapacity - 1);
		}
		return mImguiSrvHeapBase + mImguiNextSlot++;
	}

	uint32_t D3D12Device::GetCurrentFrameIndex() const {
		return mSwapChain ? mSwapChain->GetCurrentBackBufferIndex() : 0;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12Device::GetSrvUavCpuHandle(
		const uint32_t absoluteSlot
	) const {
		D3D12_CPU_DESCRIPTOR_HANDLE handle =
			mSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<SIZE_T>(absoluteSlot) * mSrvUavDescriptorSize;
		return handle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE D3D12Device::GetSrvUavGpuHandle(
		const uint32_t absoluteSlot
	) const {
		D3D12_GPU_DESCRIPTOR_HANDLE handle =
			mSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<UINT64>(absoluteSlot) * mSrvUavDescriptorSize;
		return handle;
	}

	ID3D12DescriptorHeap* D3D12Device::GetRtvHeap() const {
		return mRtvHeap.Get();
	}

	uint32_t D3D12Device::GetRtvDescriptorSize() const {
		return mRtvDescriptorSize;
	}

	uint32_t D3D12Device::GetRtvHeapCapacity() const {
		return mRtvHeapCapacity;
	}

	ID3D12DescriptorHeap* D3D12Device::GetDsvHeap() const {
		return mDsvHeap.Get();
	}

	uint32_t D3D12Device::GetDsvDescriptorSize() const {
		return mDsvDescriptorSize;
	}

	uint32_t D3D12Device::GetDsvHeapCapacity() const {
		return mDsvHeapCapacity;
	}

	ID3D12RootSignature* D3D12Device::GetCsRootSignature() const {
		return mCsRootSignature.Get();
	}

	ID3D12RootSignature* D3D12Device::GetFsRootSignature() const {
		return mFsRootSignature.Get();
	}

	ID3D12RootSignature* D3D12Device::GetGeomRootSignature() const {
		return mGeomRootSignature.Get();
	}

	ID3D12RootSignature* D3D12Device::GetGeomRootSignatureLinearClamp() const {
		return mGeomRootSignatureLinearClamp.Get();
	}

	ID3D12RootSignature* D3D12Device::GetGeomRootSignaturePointClamp() const {
		return mGeomRootSignaturePointClamp.Get();
	}

	ID3D12Device* D3D12Device::GetDevice() const {
		return mDevice.Get();
	}

	DxcShaderCompiler& D3D12Device::GetDxcCompiler() {
		return mDxcCompiler;
	}

	D3D12FrameUploadAllocator& D3D12Device::GetFrameUploadAllocator() {
		const uint32_t frameIndex = mSwapChain->GetCurrentBackBufferIndex();
		return mFrames[frameIndex].upload;
	}

	uint32_t D3D12Device::GetFramesInFlight() const {
		return mFramesInFlight;
	}

	D3D12Device::UploadContext::UploadContext(D3D12Device& device) : mDevice(
		device
	) {
		auto* d = mDevice.GetDevice();

		Throw(
			d->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(
					mAllocator.ReleaseAndGetAddressOf()
				)
			)
		);

		Throw(
			d->CreateCommandList(
				0, D3D12_COMMAND_LIST_TYPE_DIRECT, mAllocator.Get(), nullptr,
				IID_PPV_ARGS(mCommandList.ReleaseAndGetAddressOf())
			)
		);

		// コマンドリストは開いた状態で作成されるので閉じる
		Throw(mCommandList->Close());

		Throw(
			d->CreateFence(
				0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(
					mFence.ReleaseAndGetAddressOf()
				)
			)
		);

		mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!mFenceEvent) {
			Fatal("UploadContext", "Failed to create upload fence event.");
		}
	}

	D3D12Device::UploadContext::~UploadContext() {
		if (mFenceEvent) {
			CloseHandle(mFenceEvent);
			mFenceEvent = nullptr;
		}
	}

	void D3D12Device::UploadContext::Begin() {
		Throw(mAllocator->Reset());
		Throw(mCommandList->Reset(mAllocator.Get(), nullptr));
	}

	uint64_t D3D12Device::UploadContext::EndAndSubmitAndWait() {
		Throw(mCommandList->Close());

		ID3D12CommandList* lists[] = {mCommandList.Get()};
		auto*              queue   = mDevice.GetGraphicsQueue();
		queue->ExecuteCommandLists(1, lists);

		const uint64_t signalValue = ++mFenceValue;
		Throw(queue->Signal(mFence.Get(), signalValue));

		if (mFence->GetCompletedValue() < signalValue) {
			Throw(mFence->SetEventOnCompletion(signalValue, mFenceEvent));
			WaitForSingleObject(mFenceEvent, INFINITE);
		}
		return signalValue;
	}

	D3D12Device::UploadContext& D3D12Device::GetUploadContext() {
		if (!mUploadContext) {
			mUploadContext = std::make_unique<UploadContext>(*this);
		}
		return *mUploadContext;
	}

	void D3D12Device::EnableDebugLayer(const bool gpuValidation) {
		ComPtr<ID3D12Debug> debugController;
		if (
			SUCCEEDED(
				D3D12GetDebugInterface(
					IID_PPV_ARGS(debugController. GetAddressOf())
				)
			)
		) {
			debugController->EnableDebugLayer();

			if (gpuValidation) {
				ComPtr<ID3D12Debug1> debugController1;
				if (SUCCEEDED(debugController.As(&debugController1))) {
					debugController1->SetEnableGPUBasedValidation(TRUE);
				}
			}
		}
	}

	void D3D12Device::CreateDevice() {
		// --- ファクトリーの生成 ---
		UINT factoryFlags = 0;
#ifdef _DEBUG
		factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

		Throw(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&mFactory)));

		// --- アダプタの確認 ---
		constexpr std::array gpuPreferences = {
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			DXGI_GPU_PREFERENCE_MINIMUM_POWER,
			DXGI_GPU_PREFERENCE_UNSPECIFIED,
		};

		ComPtr<IDXGIAdapter4> adapter;
		if (const auto factory6 = QueryInterface<IDXGIFactory6>(mFactory)) {
			bool found = false;
			for (const auto preference : gpuPreferences) {
				UINT i = 0; // アダプタ列挙用インデックス
				while (
					factory6->EnumAdapterByGpuPreference(
						i, preference, IID_PPV_ARGS(&adapter)
					) != DXGI_ERROR_NOT_FOUND
				) {
					// アダプタの情報を取得
					DXGI_ADAPTER_DESC3 adapterDesc3;
					Throw(adapter->GetDesc3(&adapterDesc3));
					Msg(
						kChannel, "Found Adapter: {}",
						StrUtil::ToString(adapterDesc3.Description)
					);
					if (!(adapterDesc3.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
						SpecialMsg(
							LogLevel::None,
							kChannel,
							"	Use Adapter: {}",
							StrUtil::ToString(adapterDesc3.Description)
						);
						SpecialMsg(
							LogLevel::None,
							kChannel,
							"	GPU Details: Vendor ID: 0x{:X}, Device ID: 0x{:X}",
							adapterDesc3.VendorId, adapterDesc3.DeviceId
						);
						SpecialMsg(
							LogLevel::None,
							kChannel,
							"	GPU Memory: Dedicated Video: {:.2f} MB, Dedicated System: {:.2f} MB, Shared System: {:.2f} MB",
							static_cast<float>(
								adapterDesc3.DedicatedVideoMemory
							) / (1024.0f * 1024.0f),
							static_cast<float>(
								adapterDesc3. DedicatedSystemMemory
							) / (1024.0f * 1024.0f),
							static_cast<float>(
								adapterDesc3.SharedSystemMemory
							) / (1024.0f * 1024.0f)
						);
						SpecialMsg(
							LogLevel::None,
							kChannel,
							"	GPU Revision: {}, SubSys ID: 0x{:X}",
							adapterDesc3.Revision,
							adapterDesc3.SubSysId
						);
						found = true;
						break;
					}
					adapter.Reset();
					++i;
				}
				if (found) {
					break;
				}
			}
			UASSERT(adapter != nullptr && "適切なアダプタが見つかりませんでした");
		} else {
			Fatal(kChannel, "それはちょっとねぇ、世間は許してくrえゃすぇんよ");
			UASSERT(false && "IDXGIFactory6の取得に失敗しました");
		}

		// --- アダプタ変更イベントの登録（できれば）---
		ComPtr<IDXGIFactory7> factory7;
		if (SUCCEEDED(mFactory.As(&factory7))) {
			mFactory7           = factory7;
			mAdapterChangeEvent = CreateEventW(
				nullptr, FALSE, FALSE, nullptr
			);
			UASSERT(mAdapterChangeEvent != nullptr && "アダプタ変更イベントの作成に失敗しました");

			Throw(
				mFactory7->RegisterAdaptersChangedEvent(
					mAdapterChangeEvent, &mAdapterChangeEventCookie
				)
			);

			DevMsg(kChannel, "アダプタ変更イベントが登録されました");
		}

		// 最新の機能レベルから順に対応しているか確認
		constexpr std::array featureLevels = {
			D3D_FEATURE_LEVEL_12_2,
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
		};

		// --- デバイスの生成 ---
		for (size_t i = 0; i < featureLevels.size(); ++i) {
			const HRESULT hr = D3D12CreateDevice(
				adapter.Get(),
				featureLevels[i],
				IID_PPV_ARGS(mDevice.ReleaseAndGetAddressOf())
			);

			if (SUCCEEDED(hr)) {
				constexpr std::array featureLevelStr = {
					"12.2", "12.1", "12.0", "11.1", "11.0",
				};
				Msg(
					kChannel,
					"Created D3D12 device with feature level {}",
					featureLevelStr[i]
				);
				break;
			}

			// 失敗したら次へ
			mDevice.Reset();
		}

		UASSERT(mDevice != nullptr && "D3D12デバイスの作成に失敗しました");
	}

	void D3D12Device::EnableValidationLayer() const {
		ComPtr<ID3D12InfoQueue> queue;
		if (SUCCEEDED(mDevice->QueryInterface(IID_PPV_ARGS(&queue)))) {
			// 致命的なエラー時に止める
			queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			// エラー時に止める
			queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			// 警告時に止める
			queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

			std::array denyIds = {
				// Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
				// https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
				D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
				// これはオーバーレイ系を使うと出るので抑制しておく
				D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED,

				// お口チャック
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
			};

			// 抑制するレベル
			std::array              severities = {D3D12_MESSAGE_SEVERITY_INFO};
			D3D12_INFO_QUEUE_FILTER filter = {};
			filter.DenyList.NumIDs = static_cast<UINT>(denyIds.size());
			filter.DenyList.pIDList = denyIds.data();
			filter.DenyList.NumSeverities =
				static_cast<UINT>(severities.size());
			filter.DenyList.pSeverityList = severities.data();
			// 抑制
			Throw(queue->PushStorageFilter(&filter));
		}
	}

	void D3D12Device::CreateQueue() {
		// コマンドキューの生成
		D3D12_COMMAND_QUEUE_DESC desc;
		desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 0;

		Throw(
			mDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&mGraphicsQueue))
		);

		Throw(mGraphicsQueue->SetName(L"GraphicsQueue"));

		Msg(kChannel, "Created Graphics Command Queue for direct commands");
	}

	void D3D12Device::CreateSrvUavHeap() {
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 8192; // とりあえず大きめに確保
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		mSrvUavTotalCapacity = desc.NumDescriptors;
		mImguiSrvHeapCapacity = 2048;
		UASSERT(mSrvUavTotalCapacity > mImguiSrvHeapCapacity);
		mSrvUavHeapCapacity = mSrvUavTotalCapacity - mImguiSrvHeapCapacity;
		mImguiSrvHeapBase   = mSrvUavHeapCapacity;
		mImguiNextSlot      = 0;

		Throw(
			mDevice->CreateDescriptorHeap(
				&desc, IID_PPV_ARGS(mSrvUavHeap.ReleaseAndGetAddressOf())
			)
		);
		mSrvUavDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

		DevMsg(
			kChannel,
			"Created SRV/UAV heap: total={}, render={}, imgui={}, imguiBase={}",
			mSrvUavTotalCapacity,
			mSrvUavHeapCapacity,
			mImguiSrvHeapCapacity,
			mImguiSrvHeapBase
		);
	}

	void D3D12Device::CreateRtvHeap() {
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.NumDescriptors             = 1024; // 必要に応じて増やす
		desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		mRtvHeapCapacity = desc.NumDescriptors; // キャパシティを保存

		Throw(
			mDevice->CreateDescriptorHeap(
				&desc, IID_PPV_ARGS(mRtvHeap.ReleaseAndGetAddressOf())
			)
		);
		mRtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV
		);
	}

	void D3D12Device::CreateDsvHeap() {
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		desc.NumDescriptors             = 1024; // 必要に応じて増やす
		desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		mDsvHeapCapacity = desc.NumDescriptors;

		Throw(
			mDevice->CreateDescriptorHeap(
				&desc, IID_PPV_ARGS(mDsvHeap.ReleaseAndGetAddressOf())
			)
		);
		mDsvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_DSV
		);
	}

	void D3D12Device::CreatePipelines() {
		CreateComputeRootSignature();
		CreateFullscreenRootSignature();
		CreateGeometryRootSignature();
	}

	void D3D12Device::CreateComputeRootSignature() {
		// コンピュートルートシグネチャ UAV(u0)
		{
			D3D12_DESCRIPTOR_RANGE range = {};
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			range.NumDescriptors = 1;
			range.BaseShaderRegister = 0; // u0
			range.RegisterSpace = 0;
			range.OffsetInDescriptorsFromTableStart =
				D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_ROOT_PARAMETER param = {};
			param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			param.DescriptorTable.NumDescriptorRanges = 1;
			param.DescriptorTable.pDescriptorRanges = &range;
			param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			D3D12_ROOT_SIGNATURE_DESC desc = {};
			desc.NumParameters             = 1;
			desc.pParameters               = &param;
			desc.Flags                     = D3D12_ROOT_SIGNATURE_FLAG_NONE;

			ComPtr<ID3DBlob> blob, error;
			Throw(
				D3D12SerializeRootSignature(
					&desc,
					D3D_ROOT_SIGNATURE_VERSION_1,
					blob.ReleaseAndGetAddressOf(),
					error.ReleaseAndGetAddressOf()
				)
			);
			Throw(
				mDevice->CreateRootSignature(
					0,
					blob->GetBufferPointer(),
					blob->GetBufferSize(),
					IID_PPV_ARGS(mCsRootSignature.ReleaseAndGetAddressOf())
				)
			);
		}
	}

	void D3D12Device::CreateFullscreenRootSignature() {
		// フルスクリーン ルートシグネチャ
		// CBV(b0)=PostFxParams, SRV(t0)=SourceTexture, static sampler(s0)
		{
			D3D12_DESCRIPTOR_RANGE range = {};
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			range.NumDescriptors = 1;
			range.BaseShaderRegister = 0; // t0
			range.RegisterSpace = 0;
			range.OffsetInDescriptorsFromTableStart =
				D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			std::array<D3D12_ROOT_PARAMETER, 2> param = {};
			param[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			param[0].Descriptor.ShaderRegister = 0; // b0
			param[0].Descriptor.RegisterSpace = 0;
			param[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			param[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			param[1].DescriptorTable.NumDescriptorRanges = 1;
			param[1].DescriptorTable.pDescriptorRanges = &range;
			param[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_STATIC_SAMPLER_DESC sampler = {};
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.MipLODBias = 0.0f;
			sampler.MinLOD = 0.0f;
			sampler.MaxLOD = D3D12_FLOAT32_MAX;
			sampler.ShaderRegister = 0; // s0
			sampler.RegisterSpace = 0;
			sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_ROOT_SIGNATURE_DESC desc = {};
			desc.NumParameters             = static_cast<UINT>(param.size());
			desc.pParameters               = param.data();
			desc.NumStaticSamplers         = 1;
			desc.pStaticSamplers           = &sampler;

			// フルスクリーン用なので IA を許可
			desc.Flags =
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			ComPtr<ID3DBlob> blob, error;
			Throw(
				D3D12SerializeRootSignature(
					&desc, D3D_ROOT_SIGNATURE_VERSION_1,
					blob.ReleaseAndGetAddressOf(),
					error.ReleaseAndGetAddressOf()
				)
			);
			Throw(
				mDevice->CreateRootSignature(
					0,
					blob->GetBufferPointer(),
					blob->GetBufferSize(),
					IID_PPV_ARGS(mFsRootSignature.ReleaseAndGetAddressOf())
				)
			);
		}
	}

	void D3D12Device::CreateGeometryRootSignature() {
		// Geometry RootSignature:
		// CBV(b0)=Frame, CBV(b1)=Object, CBV(b2)=Material, CBV(b3)=Skinning, SRV(t0)=BaseColor
		{
			auto createGeomSignature = [this](
				                         ID3D12RootSignature** outSignature,
				                         const D3D12_FILTER    filter,
				                         const D3D12_TEXTURE_ADDRESS_MODE
				                             addressMode
			                         ) {
			D3D12_DESCRIPTOR_RANGE srvRange = {};
			srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			srvRange.NumDescriptors = 1;
			srvRange.BaseShaderRegister = 0; // t0
			srvRange.RegisterSpace = 0;
			srvRange.OffsetInDescriptorsFromTableStart =
				D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			// CBV(b0)->FrameConstants, CBV(b1)->ObjectConstants, CBV(b2)->Material, CBV(b3)->SkinningPalette
			std::array<D3D12_ROOT_PARAMETER, 5> param = {};
			param[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			param[0].Descriptor.ShaderRegister = 0; // b0
			param[0].Descriptor.RegisterSpace = 0;
			param[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			param[1].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
			param[1].Descriptor.ShaderRegister = 1; // b1
			param[1].Descriptor.RegisterSpace  = 0;
			param[1].ShaderVisibility          = D3D12_SHADER_VISIBILITY_VERTEX;

			param[2].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
			param[2].Descriptor.ShaderRegister = 2; // b2
			param[2].Descriptor.RegisterSpace  = 0;
			param[2].ShaderVisibility          = D3D12_SHADER_VISIBILITY_PIXEL;

			param[3].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
			param[3].Descriptor.ShaderRegister = 3; // b3
			param[3].Descriptor.RegisterSpace  = 0;
			param[3].ShaderVisibility          = D3D12_SHADER_VISIBILITY_VERTEX;

			param[4].ParameterType =
				D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			param[4].DescriptorTable.NumDescriptorRanges = 1;
			param[4].DescriptorTable.pDescriptorRanges = &srvRange;
			param[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_STATIC_SAMPLER_DESC sampler = {};
			sampler.Filter = filter;
			sampler.AddressU = addressMode;
			sampler.AddressV = addressMode;
			sampler.AddressW = addressMode;
			sampler.MinLOD = 0.0f;
			sampler.MaxLOD = D3D12_FLOAT32_MAX;
			sampler.MaxAnisotropy = filter == D3D12_FILTER_ANISOTROPIC ? 16 : 1;
			sampler.MipLODBias = 0.0f;
			sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			sampler.ShaderRegister = 0; // s0
			sampler.RegisterSpace = 0;
			sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_ROOT_SIGNATURE_DESC desc = {};
			desc.NumParameters             = static_cast<UINT>(param.size());
			desc.pParameters               = param.data();
			desc.NumStaticSamplers         = 1;
			desc.pStaticSamplers           = &sampler;
			desc.Flags                     =
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			ComPtr<ID3DBlob> blob, error;
			Throw(
				D3D12SerializeRootSignature(
					&desc,
					D3D_ROOT_SIGNATURE_VERSION_1,
					blob.ReleaseAndGetAddressOf(),
					error.ReleaseAndGetAddressOf()
				)
			);

			Throw(
				mDevice->CreateRootSignature(
					0,
					blob->GetBufferPointer(),
					blob->GetBufferSize(),
					IID_PPV_ARGS(outSignature)
				)
			);
			};

			createGeomSignature(
				mGeomRootSignature.ReleaseAndGetAddressOf(),
				D3D12_FILTER_ANISOTROPIC,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP
			);
			createGeomSignature(
				mGeomRootSignatureLinearClamp.ReleaseAndGetAddressOf(),
				D3D12_FILTER_MIN_MAG_MIP_LINEAR,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP
			);
			createGeomSignature(
				mGeomRootSignaturePointClamp.ReleaseAndGetAddressOf(),
				D3D12_FILTER_MIN_MAG_MIP_POINT,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP
			);
		}
	}

	void D3D12Device::CreateCommandObjects() {
		for (uint32_t i = 0; i < mFramesInFlight; ++i) {
			Throw(
				mDevice->CreateCommandAllocator(
					D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(
						mFrames[i].commandAllocator.ReleaseAndGetAddressOf()
					)
				)
			);

			// 256KBのアップロードアロケータを作成
			mFrames[i].upload.Initialize(mDevice.Get(), 256 * 1024);

			mFrames[i].fenceValue = 0;
		}

		Throw(
			mDevice->CreateCommandList(
				0,
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				mFrames[0].commandAllocator.Get(),
				nullptr,
				IID_PPV_ARGS(
					mCommandList.ReleaseAndGetAddressOf()
				)
			)
		);

		// 作成時には開いているので閉じておく
		Throw(mCommandList->Close());
	}

	void D3D12Device::CreateFenceObjects() {
		Throw(
			mDevice->CreateFence(
				0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(
					mFence.ReleaseAndGetAddressOf()
				)
			)
		);
		mFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!mFenceEvent) {
			throw std::runtime_error("Create Event failed.");
		}
		mNextFenceValue = 1;
	}

	void D3D12Device::WaitForFrame(const uint32_t frameIndex) const {
		const uint64_t fenceValue = mFrames[frameIndex].fenceValue;
		if (fenceValue == 0) {
			return;
		}

		if (mFence->GetCompletedValue() < fenceValue) {
			Throw(mFence->SetEventOnCompletion(fenceValue, mFenceEvent));
			WaitForSingleObject(mFenceEvent, INFINITE);
		}
	}

	void D3D12Device::SignalFrame(const uint32_t frameIndex) {
		const uint64_t fenceValue      = mNextFenceValue++;
		mFrames[frameIndex].fenceValue = fenceValue;
		Throw(mGraphicsQueue->Signal(mFence.Get(), fenceValue));
	}

	void D3D12Device::WaitForGpuIdle() {
		// すべてのフレームが完了するまで待機
		for (uint32_t i = 0; i < mFramesInFlight; ++i) {
			SignalFrame(i);
		}
		for (uint32_t i = 0; i < mFramesInFlight; ++i) {
			WaitForFrame(i);
		}
	}

	uint64_t D3D12Device::GetCompletedFenceValue() const {
		return mFence ? mFence->GetCompletedValue() : 0;
	}

	uint64_t D3D12Device::GetCurrentFrameFenceValue() const {
		if (!mSwapChain) {
			return 0;
		}
		return GetLastSubmittedFenceValue(
			mSwapChain->GetCurrentBackBufferIndex()
		);
	}

	uint64_t D3D12Device::GetLastSubmittedFenceValue(
		const uint32_t frameIndex
	) const {
		if (frameIndex >= mFramesInFlight) {
			return 0;
		}
		return mFrames[frameIndex].fenceValue;
	}

	uint64_t D3D12Device::GetNextSignalFenceValue() const {
		return mNextFenceValue;
	}
}
