#include <cassert>
#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXTex.h>
#include <dxgi1_3.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <format>

#include <engine/public/OldConsole/Console.h>
#include <engine/public/OldConsole/ConVar.h>
#include <engine/public/OldConsole/ConVarManager.h>
#include <engine/public/renderer/D3D12.h>
#include <engine/public/renderer/SrvManager.h>
#include <engine/public/utils/StrUtil.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

using namespace Microsoft::WRL;

constexpr Vec4 kClearColorSwapChain = Vec4(0.0f, 0.0f, 0.0f, 1.0f);

D3D12::D3D12(BaseWindow* window) : mWindow(window) {
#ifdef _DEBUG
	//EnableDebugLayer();
#endif
	CreateDevice();

#ifdef _DEBUG
	//SetInfoQueueBreakOnSeverity();
#endif

	CreateCommandQueue();
	CreateSwapChain();
	CreateDescriptorHeaps();
	CreateRTV();
	CreateDSV();

	mDefaultDepthStencilTexture = CreateDepthStencilTexture(
		mWindow->GetClientWidth(), mWindow->GetClientHeight());

	CreateCommandAllocator();
	CreateCommandList();
	CreateFence();
	Resize(kClientWidth, kClientHeight);

	SetViewportAndScissor();

	Console::Print("Complete Init D3D12.\n", kConTextColorCompleted,
	               Channel::Engine);
}

D3D12::~D3D12() {
	CloseHandle(mFenceEvent);
}

void D3D12::Init() {
	ConVarManager::RegisterConVar<bool>("r_clear", true, "Clear the screen",
	                                    ConVarFlags::ConVarFlags_Notify);
	ConVarManager::RegisterConVar<int>("r_vsync", 0, "Enable VSync",
	                                   ConVarFlags::ConVarFlags_Notify);
}

void D3D12::Shutdown() {
	PrepareForShutdown();

	// GPUの処理完了を待機
	if (mCommandQueue && mFence) {
		// 実行中のコマンドの完了を待機
		WaitPreviousFrame();

		// 最後のフェンス同期
		const UINT64 lastFenceValue = mFenceValue;
		mCommandQueue->Signal(mFence.Get(), lastFenceValue);
		if (mFence->GetCompletedValue() < lastFenceValue) {
			mFence->SetEventOnCompletion(lastFenceValue, mFenceEvent);
			WaitForSingleObject(mFenceEvent, INFINITE);
		}
	}

	// コマンドリストのクリーンアップ
	if (mCommandList) {
		// すべての処理が終了するまで待機
		WaitPreviousFrame();
		// コマンドリストをリセット
		mCommandList.Reset();
	}

	// コマンドアロケータのクリーンアップ
	if (mCommandAllocator) {
		mCommandAllocator->Reset();
		mCommandAllocator.Reset();
	}

	// レンダーターゲットの解放
	for (auto& rt : mRenderTargets) {
		if (rt) {
			rt.Reset();
		}
	}
	mRenderTargets.clear();
	mRtvHandlesSwapChain.clear();

	// 深度ステンシルリソースの解放
	if (mDefaultDepthStencilTexture.dsv) {
		mDefaultDepthStencilTexture.dsv.Reset();
	}

	// ディスクリプタヒープの解放
	if (mRtvDescriptorHeap) {
		mRtvDescriptorHeap.Reset();
	}
	if (mDsvDescriptorHeap) {
		mDsvDescriptorHeap.Reset();
	}

	// フェンス関連の解放
	if (mFenceEvent) {
		CloseHandle(mFenceEvent);
		mFenceEvent = nullptr;
	}
	if (mFence) {
		mFence.Reset();
	}


	// コマンドキューの解放
	if (mCommandQueue) {
		mCommandQueue.Reset();
	}

	// スワップチェーンの解放
	if (mSwapChain) {
		BOOL isFullScreen = FALSE;
		mSwapChain->GetFullscreenState(&isFullScreen, nullptr);
		if (isFullScreen) {
			mSwapChain->SetFullscreenState(FALSE, nullptr);
		}
		mSwapChain.Reset();
	}

	// DXGIファクトリーの解放
	if (mDxgiFactory) {
		mDxgiFactory.Reset();
	}

	// デバイスの解放（最後に行う）
	if (mDevice) {
		mDevice.Reset();
	}
}

void D3D12::SetShaderResourceViewManager(
	SrvManager* srvManager) {
	mSrvManager = srvManager;
}

void D3D12::ClearColorAndDepth() {
	auto dsvHandle = mDefaultDepthStencilTexture.dsvHandle;

	mCommandList->ClearDepthStencilView(
		dsvHandle,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0,
		0,
		nullptr
	);

	mCommandList->OMSetRenderTargets(
		1,
		&mRtvHandlesSwapChain[mFrameIndex],
		false,
		&dsvHandle
	);

	if (ConVarManager::GetConVar("r_clear")->GetValueAsBool()) {
		const float clearColor[] = {
			kClearColorSwapChain.x,
			kClearColorSwapChain.y,
			kClearColorSwapChain.z,
			kClearColorSwapChain.w
		};

		mCommandList->ClearRenderTargetView(
			mRtvHandlesSwapChain[mFrameIndex],
			clearColor,
			0,
			nullptr
		);
	}

	SetViewportAndScissor(
		mWindow->GetClientWidth(),
		mWindow->GetClientHeight()
	);
}

void D3D12::PreRender() {
	//WaitPreviousFrame();

	// これから書き込むバックバッファのインデックスを取得
	mFrameIndex = mSwapChain->GetCurrentBackBufferIndex();

	// コマンドのリセット
	HRESULT hr = mCommandAllocator->Reset();
	assert(SUCCEEDED(hr));
	hr = mCommandList->Reset(
		mCommandAllocator.Get(),
		nullptr
	);
	assert(SUCCEEDED(hr));

	// リソースバリアを張る
	mBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	mBarrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	mBarrier.Transition.pResource   = mRenderTargets[mFrameIndex].Get();
	mBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	mBarrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
	mCommandList->ResourceBarrier(1, &mBarrier);

	// レンダーターゲットと深度ステンシルバッファをクリアする
	ClearColorAndDepth();

	// ビューポートとシザー矩形を設定
	mCommandList->RSSetViewports(1, &mViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);
}

void D3D12::PostRender() {
	//-------------------------------------------------------------------------
	// 描画終了 ↑
	//-------------------------------------------------------------------------

	// リソースバリア遷移↓
	mBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	mBarrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
	mCommandList->ResourceBarrier(1, &mBarrier);

	// コマンドリストを閉じる
	if (const HRESULT hr = mCommandList->Close()) {
		Console::Print(std::format("{:08x}\n", hr), kConTextColorError);
		assert(SUCCEEDED(hr));
	}

	// コマンドのキック
	mCommandQueue->ExecuteCommandLists(
		1, CommandListCast(mCommandList.GetAddressOf()));

	// GPU と OS に画面の交換を行うよう通知
	mSwapChain->Present(ConVarManager::GetConVar("r_vsync")->GetValueAsInt(),
	                    0);

	WaitPreviousFrame(); // 前のフレームを待つ
}

void D3D12::BeginRenderPass(const RenderPassTargets& targets) const {
	mCommandList->OMSetRenderTargets(
		targets.numRTVs,
		targets.pRTVs,
		false,
		targets.pDSV
	);

	if (ConVarManager::GetConVar("r_clear")->GetValueAsBool()) {
		if (targets.bClearColor) {
			FLOAT clearColor[4] = {
				targets.clearColor.x,
				targets.clearColor.y,
				targets.clearColor.z,
				targets.clearColor.w
			};
			for (uint32_t i = 0; i < targets.numRTVs; ++i) {
				mCommandList->ClearRenderTargetView(
					targets.pRTVs[i],
					clearColor,
					0,
					nullptr
				);
			}
		}
	}

	if (targets.bClearDepth) {
		mCommandList->ClearDepthStencilView(
			*targets.pDSV,
			D3D12_CLEAR_FLAG_DEPTH,
			targets.clearDepth,
			targets.clearStencil,
			0,
			nullptr
		);
	}

	// commandList_->RSSetViewports(1, &viewport_);
	// commandList_->RSSetScissorRects(1, &scissorRect_);
}

void D3D12::BeginSwapChainRenderPass() {
	ClearColorAndDepth();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::GetSwapChainRenderTargetView() const {
	return mRtvHandlesSwapChain[mFrameIndex];
}

void D3D12::ResetCommandList() {
	// 前のコマンドリストの実行が完了するのを待つ
	WaitPreviousFrame();

	// コマンドアロケータとコマンドリストをリセット
	HRESULT hr = mCommandAllocator->Reset();
	if (FAILED(hr)) {
		// エラー処理
		return;
	}

	hr = mCommandList->Reset(mCommandAllocator.Get(), nullptr);
	if (FAILED(hr)) {
		// エラー処理
		return;
	}
}

void D3D12::WriteToUploadHeapMemory(ID3D12Resource* resource,
                                    const uint32_t  size, const void* data) {
	void*   mapped;
	HRESULT hr = resource->Map(0, nullptr, &mapped);
	if (SUCCEEDED(hr)) {
		memcpy(mapped, data, size);
		resource->Unmap(0, nullptr);
	}
}

void D3D12::WaitPreviousFrame() {
	mFenceValue++; // Fenceの値を更新

	// GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
	if (mCommandQueue && mFence) {
		const HRESULT hr = mCommandQueue->Signal(mFence.Get(), mFenceValue);
		if (FAILED(hr)) {
			if (hr == DXGI_ERROR_DEVICE_REMOVED || hr ==
				DXGI_ERROR_DEVICE_RESET) {
				// デバイスが消えた!!?
				HandleDeviceLost();
			}
		}

		// Fenceの値が指定したSignal値にたどり着いているか確認する
		if (mFence->GetCompletedValue() < mFenceValue) {
			// 指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
			mFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
			// イベント待つ
			WaitForSingleObject(mFenceEvent, INFINITE);
		}
	}
}

void D3D12::Flush() {
	UINT64 fenceValue = ++mFenceValue;
	mCommandQueue->Signal(mFence.Get(), fenceValue);
	if (mFence->GetCompletedValue() < fenceValue) {
		HANDLE event = CreateEvent(nullptr, false, false, nullptr);
		mFence->SetEventOnCompletion(fenceValue, event);
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}
}

void D3D12::Resize(const uint32_t width, const uint32_t height) {
	if (width == 0 || height == 0) {
		return;
	}

	// リソースを開放
	for (auto& rt : mRenderTargets) {
		rt.Reset();
	}
	mRenderTargets.clear();
	mDefaultDepthStencilTexture.dsv.Reset();

	// ディスクリプタヒープを解放
	mRtvDescriptorHeap.Reset();
	mDsvDescriptorHeap.Reset();

	// スワップチェーンのサイズを変更
	HRESULT hr = mSwapChain->ResizeBuffers(
		kFrameBufferCount,
		width, height,
		DXGI_FORMAT_UNKNOWN,
		0
	);

	if (FAILED(hr)) {
		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
			HandleDeviceLost();
			return;
		}
		Console::Print(
			std::format("Failed to resize swap chain. Error code: {:08x}\n",
			            hr),
			kConTextColorError,
			Channel::Engine
		);
		return;
	}

	// ディスクリプタヒープを再作成
	CreateDescriptorHeaps();

	// レンダーターゲットを再作成
	CreateRTV();

	// デプスステンシルバッファを再作成
	CreateDSV();

	// 再作成
	mDefaultDepthStencilTexture = CreateDepthStencilTexture(
		width, height, DXGI_FORMAT_D32_FLOAT);

	//// ビューポートとシザー矩形を再設定
	//SetViewportAndScissor(window_->GetClientWidth(),
	//	window_->GetClientHeight());
}

void D3D12::ResetOffscreenRenderTextures() {
	mRtvHandlesOffscreen.clear();
	mCurrentDsvIndex = 0;
}

void D3D12::SetViewportAndScissor(const uint32_t width, const uint32_t height) {
	mViewport.TopLeftX = 0;
	mViewport.TopLeftY = 0;
	mViewport.Width    = static_cast<FLOAT>(width);
	mViewport.Height   = static_cast<FLOAT>(height);
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;

	mScissorRect.left   = 0;
	mScissorRect.top    = 0;
	mScissorRect.right  = static_cast<LONG>(width);
	mScissorRect.bottom = static_cast<LONG>(height);

	// ビューポートとシザー矩形を設定
	mCommandList->RSSetViewports(1, &mViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);
}

void D3D12::EnableDebugLayer() {
	ID3D12Debug6* debugController;

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();                // デバッグレイヤーの有効化
		debugController->SetEnableGPUBasedValidation(TRUE); // GPU側でのチェックを有効化
		debugController->Release();
	}
}

void D3D12::CreateDevice() {
	// ファクトリーの生成
	HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&mDxgiFactory));
	assert(SUCCEEDED(hr));

	// ハードウェアアダプタの検索
	ComPtr<IDXGIAdapter4> useAdapter;
	for (UINT i = 0; mDxgiFactory->EnumAdapterByGpuPreference(
		     i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)
	     ) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC3 adapterDesc;
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));
		// ソフトウェアアダプタでなければ採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			// 採用したアダプタの情報をログに出力
			Console::Print(StrUtil::ToString(
				std::format(L"Use Adapter : {}\n", adapterDesc.Description)));
			break;
		}
		useAdapter = nullptr; // ソフトウェアアダプタの場合は見なかったことにする
	}
	assert(useAdapter != nullptr); // 適切なアダプタが見つからなかったので起動できない

	// D3D12デバイスの生成
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	const char* featureLevelStrings[] = {
		"12.2", "12.1", "12.0", "11.1", "11.0"
	};

	// 高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		// 採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i],
		                       IID_PPV_ARGS(&mDevice));
		// 指定した機能レベルでデバイスが生成できたをできたかを確認
		if (SUCCEEDED(hr)) {
			// 生成できたのでログに出力し、ループを抜ける
			Console::Print(
				std::format("FeatureLevel : {}\n", featureLevelStrings[i]),
				kConFgColorDark);
			break;
		}
		mDevice = nullptr;
	}
	assert(mDevice != nullptr); // デバイスの生成がうまくいかなかったので起動できない

	mDevice->SetName(L"DX12Device");
}

void D3D12::SetInfoQueueBreakOnSeverity() const {
	ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(mDevice->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		// やばいエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		// エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		// 警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		// 抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {
			// Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
			// https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
			D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED
		};

		// 抑制するレベル
		D3D12_MESSAGE_SEVERITY  severities[] = {D3D12_MESSAGE_SEVERITY_INFO};
		D3D12_INFO_QUEUE_FILTER filter       = {};
		filter.DenyList.NumIDs               = _countof(denyIds);
		filter.DenyList.pIDList              = denyIds;
		filter.DenyList.NumSeverities        = _countof(severities);
		filter.DenyList.pSeverityList        = severities;
		// 指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);
	}
}

void D3D12::CreateCommandQueue() {
	// コマンドキューの生成
	constexpr D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		0,
		D3D12_COMMAND_QUEUE_FLAG_NONE,
		0
	};
	if (const HRESULT hr = mDevice->CreateCommandQueue(
		&commandQueueDesc, IID_PPV_ARGS(&mCommandQueue))) {
		Console::Print(std::format("{:08x}\n", hr), kConTextColorError);
		assert(SUCCEEDED(hr));
	}
	mCommandQueue->SetName(L"DX12CommandQueue");
}

void D3D12::CreateSwapChain() {
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount           = kFrameBufferCount;
	swapChainDesc.Width                 = mWindow->GetClientWidth();
	swapChainDesc.Height                = mWindow->GetClientHeight();
	swapChainDesc.Format                = kBufferFormat; // 色の形式
	swapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	// レンダーターゲットとして利用
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニタに映したら中身を破棄
	swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	if (mWindow->GetWindowHandle()) {
		swapChainDesc.Scaling   = DXGI_SCALING_STRETCH;   // 画面サイズに合わせて伸縮
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE; // アルファモードは無視
		Console::Print("Window Mode\n", kConFgColorDark);
	} else {
		swapChainDesc.Scaling   = DXGI_SCALING_NONE;             // 画面サイズに合わせない
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED; // アルファモードは事前乗算
		Console::Print("FullScreen Mode\n", kConFgColorDark);
	}

	const HRESULT hr = mDxgiFactory->CreateSwapChainForHwnd(
		mCommandQueue.Get(),
		mWindow->GetWindowHandle(),
		&swapChainDesc,
		nullptr,
		nullptr,
		reinterpret_cast<IDXGISwapChain1**>(mSwapChain.GetAddressOf())
	);

	if (hr) {
		Console::Print(std::format("{:08x}\n", hr), kConTextColorError);
		assert(SUCCEEDED(hr));
	}
}

void D3D12::CreateDescriptorHeaps() {
	// DescriptorSizeを取得しておく
	mDescriptorSizeRtv = mDevice->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDescriptorSizeDsv = mDevice->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// RTV用のヒープでディスクリプタの数は2。RTVはShader内で触るものではないので、ShaderVisibleはfalse
	mRtvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
	                                          kFrameBufferCount +
	                                          kMaxRenderTargetCount, false);
	// DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので、ShaderVisibleはfalse
	mDsvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
	                                          kMaxRenderTargetCount, false);
}

void D3D12::CreateRTV() {
	// いらないリソースを解放
	for (auto& rtv : mRenderTargets) {
		rtv.Reset();
	}
	mRenderTargets.clear();

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {}; // RTVの設定
	rtvDesc.Format = kBufferFormat; // 出力結果を書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2Dテクスチャとして書き込む

	mRenderTargets.resize(kFrameBufferCount); // フレームバッファ数でリサイズ
	mRtvHandlesSwapChain.resize(kFrameBufferCount);

	for (unsigned int i = 0; i < kFrameBufferCount; ++i) {
		// SwapChainからResourceを引っ張ってくる
		if (const HRESULT hr = mSwapChain->GetBuffer(
			i, IID_PPV_ARGS(&mRenderTargets[i]))) {
			Console::Print(std::format("{:08x}\n", hr), kConTextColorError);
			assert(SUCCEEDED(hr));
		}

		mRtvHandlesSwapChain[i] = GetCPUDescriptorHandle(
			mRtvDescriptorHeap.Get(),
			mDescriptorSizeRtv, i);
		mDevice->CreateRenderTargetView(mRenderTargets[i].Get(), &rtvDesc,
		                                mRtvHandlesSwapChain[i]);

		mRenderTargets[i]->SetName(std::format(L"RenderTarget_{}", i).c_str());
	}
}

void D3D12::CreateDSV() {
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors             = 128; // 必要なディスクリプタ数
	desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = mDevice->CreateDescriptorHeap(
		&desc, IID_PPV_ARGS(&mDsvDescriptorHeap));
	if (FAILED(hr)) {
		Console::Print(
			std::format(
				"Failed to create DSV descriptor heap. Error code: {:08x}\n",
				hr), kConTextColorError);
		assert(SUCCEEDED(hr));
	}

	mDsvDescriptorHeap->SetName(L"DSVDescriptorHeap");

	mCurrentDsvIndex = 0;
}

void D3D12::CreateCommandAllocator() {
	const HRESULT hr = mDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocator)
	);
	if (hr) {
		Console::Print(std::format("{:08x}\n", hr), kConTextColorError);
		assert(SUCCEEDED(hr));
	}
}

void D3D12::CreateCommandList() {
	// コマンドリストの作成
	HRESULT hr = mDevice->CreateCommandList(
		0,                              // ノードマスク
		D3D12_COMMAND_LIST_TYPE_DIRECT, // コマンドリストタイプ
		mCommandAllocator.Get(),        // コマンドアロケーター
		nullptr,                        // 初期パイプラインステート
		IID_PPV_ARGS(&mCommandList)     // 作成されるコマンドリスト
	);

	mCommandList->SetName(L"MainCommandList");

	if (FAILED(hr)) {
		Console::Print(
			std::format("Failed to create command list. Error code: {:08x}\n",
			            hr),
			kConTextColorError
		);
		assert(SUCCEEDED(hr));
		return;
	}

	// コマンドリストを閉じる（初期状態では開いている）
	hr = mCommandList->Close();
	if (FAILED(hr)) {
		Console::Print(
			std::format("Failed to close command list. Error code: {:08x}\n",
			            hr),
			kConTextColorError
		);
		assert(SUCCEEDED(hr));
	}
}

void D3D12::CreateFence() {
	if (const HRESULT hr = mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
	                                            IID_PPV_ARGS(&mFence))) {
		Console::Print(std::format("{:08x}\n", hr), kConTextColorError);
		assert(SUCCEEDED(hr));
	}

	mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(mFenceEvent != nullptr);
}

void D3D12::SetViewportAndScissor() {
	// Viewport
	mViewport.TopLeftX = 0;
	mViewport.TopLeftY = 0;
	mViewport.Width    = static_cast<FLOAT>(mWindow->GetClientWidth());
	mViewport.Height   = static_cast<FLOAT>(mWindow->GetClientHeight());
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;

	// ScissorRect
	mScissorRect.left   = 0;
	mScissorRect.top    = 0;
	mScissorRect.right  = static_cast<LONG>(mWindow->GetClientWidth());
	mScissorRect.bottom = static_cast<LONG>(mWindow->GetClientHeight());
}

void D3D12::HandleDeviceLost() {
	mDevice.Reset();

	// デバイスと関連リソースの再作成
	CreateDevice();
}

//-----------------------------------------------------------------------------
// Purpose: シャットダウンに備えて準備を行います
//-----------------------------------------------------------------------------
void D3D12::PrepareForShutdown() const {
	if (mSwapChain) {
		BOOL isFullScreen = FALSE;
		mSwapChain->GetFullscreenState(&isFullScreen, nullptr);
		if (isFullScreen) {
			// フルスクリーンモードを解除して、完了するまで待機
			mSwapChain->SetFullscreenState(FALSE, nullptr);
		}
	}
}

ComPtr<ID3D12DescriptorHeap> D3D12::CreateDescriptorHeap(
	const D3D12_DESCRIPTOR_HEAP_TYPE heapType, const UINT numDescriptors,
	const bool                       shaderVisible
) const {
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC   descriptorHeapDesc = {};
	descriptorHeapDesc.Type                         = heapType;
	descriptorHeapDesc.NumDescriptors               = numDescriptors;
	if (shaderVisible) {
		descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	} else {
		descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	}
	const HRESULT hr = mDevice->CreateDescriptorHeap(
		&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	if (hr) {
		Console::Print(std::format("{:08x}\n", hr), kConTextColorError);
	}
	return descriptorHeap;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::GetCPUDescriptorHandle(
	ID3D12DescriptorHeap* descriptorHeap, const uint32_t descriptorSize,
	const uint32_t        index
) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->
		GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += static_cast<unsigned long long>(descriptorSize) * index;
	return handleCPU;
}

ComPtr<ID3D12Resource> D3D12::CreateDepthStencilTextureResource(
	uint32_t width, uint32_t height, DXGI_FORMAT format) const {
	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1; // Mipmapの数
	resourceDesc.DepthOrArraySize = 1; // 奥行きor配列Textureの配列数
	resourceDesc.Format = format; // DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	// DepthStencilとして使う通知

	D3D12_HEAP_PROPERTIES heapProperties = {}; // 利用するHeapの設定
	heapProperties.Type                  = D3D12_HEAP_TYPE_DEFAULT; // vRAM上に作る
	heapProperties.CPUPageProperty       = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	// CPUのページプロパティ
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	// メモリプールの設定

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue  = {};
	depthClearValue.DepthStencil.Depth = 1.0f; // 最大値でクリア
	depthClearValue.Format             = format;

	// Resourceの生成
	ComPtr<ID3D12Resource> resource;
	const HRESULT          hr = mDevice->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&resource)
	); // 作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	if (hr) {
		Console::Print(std::format("{:08x}\n", hr), kConTextColorError);
	}

	return resource;
}

RenderTargetTexture D3D12::CreateRenderTargetTexture(
	uint32_t width, uint32_t height, Vec4 clearColor, DXGI_FORMAT format) {
	RenderTargetTexture result = {};

	// ディスクリプタ
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment           = 0;
	desc.Width               = width;
	desc.Height              = height;
	desc.DepthOrArraySize    = 1;
	desc.MipLevels           = 1;
	desc.Format              = format;
	desc.SampleDesc.Count    = 1;
	desc.SampleDesc.Quality  = 0;
	desc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearValue    = {};
	clearValue.Format               = format;
	clearValue.DepthStencil.Depth   = 1.0f;
	clearValue.DepthStencil.Stencil = 0;
	clearValue.Color[0]             = clearColor.x;
	clearValue.Color[1]             = clearColor.y;
	clearValue.Color[2]             = clearColor.z;
	clearValue.Color[3]             = clearColor.w;

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type                  = D3D12_HEAP_TYPE_DEFAULT;

	HRESULT hr = mDevice->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&result.rtv)
	);

	if (FAILED(hr)) {
		Console::Print(std::format("{:08x}\n", hr), kConTextColorError);
		assert(SUCCEEDED(hr));
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format                        = format;
	rtvDesc.ViewDimension                 = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice            = 0;
	rtvDesc.Texture2D.PlaneSlice          = 0;

	result.rtvHandle = AllocateNewRTVHandle();
	mDevice->CreateRenderTargetView(
		result.rtv.Get(),
		&rtvDesc,
		result.rtvHandle
	);

	// SRVを作成
	if (mSrvManager) {
		// SRVのインデックスを確保（テクスチャ用）
		result.srvIndex = mSrvManager->AllocateForTexture();

		// SRVを作成
		mSrvManager->CreateSRVForTexture2D(
			result.srvIndex,
			result.rtv.Get(),
			format,
			1
		);

		// GPU用ハンドルを取得して保存
		result.srvHandleGPU = mSrvManager->GetGPUDescriptorHandle(
			result.srvIndex);

#ifdef _DEBUG
		// ハンドルの値を確認 (デバッグ用)
		Console::Print(std::format(
			               "CreateRTV: format={}, size={}x{}, srvIdx={}, srvHandleGPU.ptr=0x{:x}\n",
			               static_cast<int>(format), width, height,
			               result.srvIndex, result.srvHandleGPU.ptr),
		               kConTextColorWarning);
#endif

		// GPUハンドルが有効か確認
		if (result.srvHandleGPU.ptr == 0) {
			Console::Print("WARNING: SRV GPU handle is null!\n",
			               kConTextColorWarning);
		}
	} else {
		Console::Print("WARNING: No SrvManager available when creating RTV!\n",
		               kConTextColorWarning);
	}
	// 	srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
	// 	srvDesc.Shader4ComponentMapping         =
	// 		D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// 	srvDesc.Texture2D.MostDetailedMip     = 0;
	// 	srvDesc.Texture2D.MipLevels           = 1;
	// 	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	// 	srvDesc.Texture2D.PlaneSlice          = 0;
	//
	// 	result.srvHandles = srvManager_->RegisterShaderResourceView(
	// 		result.rtv.Get(), srvDesc
	// 	);
	//
	// 	srvManager_->CreateSRVForTexture2D(
	// 	
	// 		);
	// } else {
	// 	Console::Print("ShaderResourceViewManager is not set.\n",
	// 	               kConTextColorError);
	// }

	Console::Print("Created RenderTargetTexture.\n", kConTextColorCompleted,
	               Channel::Engine);

	return result;
}

DepthStencilTexture D3D12::CreateDepthStencilTexture(
	uint32_t width, uint32_t height, DXGI_FORMAT format) {
	DepthStencilTexture result = {};

	// 1. デプスバッファリソース作成
	result.dsv = CreateDepthStencilTextureResource(width, height, format);

	// 2. DSVディスクリプタハンドルを算出
	uint32_t dsvIndex = mCurrentDsvIndex;
	++mCurrentDsvIndex;

	auto dsvHandle = GetCPUDescriptorHandle(
		mDsvDescriptorHeap.Get(),
		mDescriptorSizeDsv,
		dsvIndex
	);
	result.dsvHandle = dsvHandle;

	// 3. DSVビュー作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format                        = format;
	dsvDesc.ViewDimension                 = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags                         = D3D12_DSV_FLAG_NONE;

	mDevice->CreateDepthStencilView(result.dsv.Get(), &dsvDesc, dsvHandle);

	return result;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::AllocateNewRTVHandle() {
	// ディスクリプタヒープのサイズを取得
	uint32_t index = kFrameBufferCount + static_cast<uint32_t>(
		mRtvHandlesSwapChain.size());

	// インデックスからハンドル取得
	auto handle = GetCPUDescriptorHandle(mRtvDescriptorHeap.Get(),
	                                     mDescriptorSizeRtv, index);

	mRtvHandlesSwapChain.emplace_back(handle);

	return handle;
}

D3DResourceLeakChecker::~D3DResourceLeakChecker() {
	// リソースリークチェック
	ComPtr<IDXGIDebug> debug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
	}
}
