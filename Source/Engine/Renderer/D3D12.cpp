#include "D3D12.h"

#include <cassert>
#include <d3d12.h>
#include <dxgi1_3.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <format>

#include <DirectXTex/d3dx12.h>

#include <Lib/Utils/ClientProperties.h>
#include <Lib/Utils/StrUtils.h>

#include <SubSystem/Console/ConVarManager.h>
#include <SubSystem/Console/Console.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

constexpr Vec4 kClearColorSwapChain = { 0.0f, 0.0f, 0.0f, 1.0f };

D3D12::D3D12(BaseWindow* window) : window_(window) {
#ifdef _DEBUG
	EnableDebugLayer();
#endif
	CreateDevice();

#ifdef _DEBUG
	SetInfoQueueBreakOnSeverity();
#endif

	CreateCommandQueue();
	CreateSwapChain();
	CreateDescriptorHeaps();
	CreateRTV();
	CreateDSV();

	defaultDepthStencilTexture_ = CreateDepthStencilTexture(window_->GetClientWidth(), window_->GetClientHeight());

	CreateCommandAllocator();
	CreateCommandList();
	CreateFence();
	Resize(kClientWidth, kClientHeight);

	SetViewportAndScissor();

	Console::Print("Complete Init D3D12.\n", kConTextColorCompleted, Channel::Engine);
}

D3D12::~D3D12() {
	CloseHandle(fenceEvent_);
	Console::Print("アリーヴェ帰ルチ! (さよナランチャ\n", kConTextColorCompleted, Channel::Engine);
}

void D3D12::Init() {
	ConVarManager::RegisterConVar<bool>("r_clear", true, "Clear the screen", ConVarFlags::ConVarFlags_Notify);
	ConVarManager::RegisterConVar<int>("r_vsync", 0, "Enable VSync", ConVarFlags::ConVarFlags_Notify);
}

void D3D12::Shutdown() {
	PrepareForShutdown();

	// GPUの処理完了を待機
	if (commandQueue_ && fence_) {
		// 実行中のコマンドの完了を待機
		WaitPreviousFrame();

		// 最後のフェンス同期
		const UINT64 lastFenceValue = fenceValue_;
		commandQueue_->Signal(fence_.Get(), lastFenceValue);
		if (fence_->GetCompletedValue() < lastFenceValue) {
			fence_->SetEventOnCompletion(lastFenceValue, fenceEvent_);
			WaitForSingleObject(fenceEvent_, INFINITE);
		}
	}

	// コマンドリストのクリーンアップ
	if (commandList_) {
		// すべての処理が終了するまで待機
		WaitPreviousFrame();
		// コマンドリストをリセット
		commandList_.Reset();
	}

	// コマンドアロケータのクリーンアップ
	if (commandAllocator_) {
		commandAllocator_->Reset();
		commandAllocator_.Reset();
	}

	// レンダーターゲットの解放
	for (auto& rt : renderTargets_) {
		if (rt) {
			rt.Reset();
		}
	}
	renderTargets_.clear();
	rtvHandles_.clear();

	// 深度ステンシルリソースの解放
	if (defaultDepthStencilTexture_.dsv) {
		defaultDepthStencilTexture_.dsv.Reset();
	}

	// ディスクリプタヒープの解放
	if (rtvDescriptorHeap_) {
		rtvDescriptorHeap_.Reset();
	}
	if (dsvDescriptorHeap_) {
		dsvDescriptorHeap_.Reset();
	}

	// フェンス関連の解放
	if (fenceEvent_) {
		CloseHandle(fenceEvent_);
		fenceEvent_ = nullptr;
	}
	if (fence_) {
		fence_.Reset();
	}


	// コマンドキューの解放
	if (commandQueue_) {
		commandQueue_.Reset();
	}

	// スワップチェーンの解放
	if (swapChain_) {
		BOOL isFullScreen = FALSE;
		swapChain_->GetFullscreenState(&isFullScreen, nullptr);
		if (isFullScreen) {
			swapChain_->SetFullscreenState(FALSE, nullptr);
		}
		swapChain_.Reset();
	}

	// DXGIファクトリーの解放
	if (dxgiFactory_) {
		dxgiFactory_.Reset();
	}

	// デバイスの解放（最後に行う）
	if (device_) {
		device_.Reset();
	}
}

void D3D12::SetShaderResourceViewManager(ShaderResourceViewManager* srvManager) {
	srvManager_ = srvManager;
}

void D3D12::ClearColorAndDepth() const {
	auto dsvHandle = defaultDepthStencilTexture_.handles.cpuHandle;

	commandList_->ClearDepthStencilView(
		dsvHandle,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0,
		0,
		nullptr
	);

	commandList_->OMSetRenderTargets(
		1,
		&rtvHandles_[frameIndex_],
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

		commandList_->ClearRenderTargetView(
			rtvHandles_[frameIndex_],
			clearColor,
			0,
			nullptr
		);
	}

	// ビューポートとシザー矩形を設定
	commandList_->RSSetViewports(1, &viewport_);
	commandList_->RSSetScissorRects(1, &scissorRect_);
}

void D3D12::PreRender() {
	//WaitPreviousFrame();

	// これから書き込むバックバッファのインデックスを取得
	frameIndex_ = swapChain_->GetCurrentBackBufferIndex();

	// コマンドのリセット
	HRESULT hr = commandAllocator_->Reset();
	assert(SUCCEEDED(hr));
	hr = commandList_->Reset(
		commandAllocator_.Get(),
		nullptr
	);
	assert(SUCCEEDED(hr));

	// リソースバリアを張る
	barrier_.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier_.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier_.Transition.pResource = renderTargets_[frameIndex_].Get();
	barrier_.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier_.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList_->ResourceBarrier(1, &barrier_);

	// レンダーターゲットと深度ステンシルバッファをクリアする
	ClearColorAndDepth();

	// ビューポートとシザー矩形を設定
	commandList_->RSSetViewports(1, &viewport_);
	commandList_->RSSetScissorRects(1, &scissorRect_);
}

void D3D12::PostRender() {
	//-------------------------------------------------------------------------
	// 描画終了 ↑
	//-------------------------------------------------------------------------

	// リソースバリア遷移↓
	barrier_.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier_.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	commandList_->ResourceBarrier(1, &barrier_);

	// コマンドリストを閉じる
	if (const HRESULT hr = commandList_->Close()) {
		Console::Print(std::format("{:08x}\n", hr), kConTextColorError);
		assert(SUCCEEDED(hr));
	}

	// コマンドのキック
	commandQueue_->ExecuteCommandLists(1, CommandListCast(commandList_.GetAddressOf()));

	// GPU と OS に画面の交換を行うよう通知
	swapChain_->Present(ConVarManager::GetConVar("r_vsync")->GetValueAsInt(), 0);

	WaitPreviousFrame(); // 前のフレームを待つ
}

void D3D12::BeginRenderPass(const RenderPassTargets& targets) const {
	commandList_->OMSetRenderTargets(
		targets.numRTVs,
		targets.pRTVs,
		false,
		targets.pDSV
	);

	if (targets.bClearColor) {
		FLOAT clearColor[4] = {
			targets.clearColor.x,
			targets.clearColor.y,
			targets.clearColor.z,
			targets.clearColor.w
		};
		for (uint32_t i = 0; i < targets.numRTVs; ++i) {
			commandList_->ClearRenderTargetView(
				targets.pRTVs[i],
				clearColor,
				0,
				nullptr
			);
		}
	}

	if (targets.bClearDepth) {
		commandList_->ClearDepthStencilView(
			*targets.pDSV,
			D3D12_CLEAR_FLAG_DEPTH,
			targets.clearDepth,
			targets.clearStencil,
			0,
			nullptr
		);
	}
}

void D3D12::BeginSwapChainRenderPass() const {
	ClearColorAndDepth();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::GetSwapChainRenderTargetView() const {
	return rtvHandles_[frameIndex_];
}

void D3D12::WriteToUploadHeapMemory(ID3D12Resource* resource, const uint32_t size, const void* data) {
	void* mapped;
	HRESULT hr = resource->Map(0, nullptr, &mapped);
	if (SUCCEEDED(hr)) {
		memcpy(mapped, data, size);
		resource->Unmap(0, nullptr);
	}
}

void D3D12::WaitPreviousFrame() {
	fenceValue_++; // Fenceの値を更新

	// GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
	if (commandQueue_ && fence_) {
		const HRESULT hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
		if (FAILED(hr)) {
			if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
				// デバイスが消えた!!?
				HandleDeviceLost();
			}
		}

		// Fenceの値が指定したSignal値にたどり着いているか確認する
		if (fence_->GetCompletedValue() < fenceValue_) {
			// 指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
			fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
			// イベント待つ
			WaitForSingleObject(fenceEvent_, INFINITE);
		}
	}
}

void D3D12::Resize(const uint32_t width, const uint32_t height) {
	if (width == 0 || height == 0) {
		return;
	}

	// リソースを開放
	for (auto& rt : renderTargets_) {
		rt.Reset();
	}
	renderTargets_.clear();
	defaultDepthStencilTexture_.dsv.Reset();

	// ディスクリプタヒープを解放
	rtvDescriptorHeap_.Reset();
	dsvDescriptorHeap_.Reset();

	// スワップチェーンのサイズを変更
	HRESULT hr = swapChain_->ResizeBuffers(
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
			std::format("Failed to resize swap chain. Error code: {:08x}\n", hr),
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
	defaultDepthStencilTexture_ = CreateDepthStencilTexture(width, height, DXGI_FORMAT_D32_FLOAT);

	// ビューポートとシザー矩形を再設定
	SetViewportAndScissor();
}

void D3D12::EnableDebugLayer() {
	ID3D12Debug6* debugController;

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer(); // デバッグレイヤーの有効化
		debugController->SetEnableGPUBasedValidation(TRUE); // GPU側でのチェックを有効化
		debugController->Release();
	}
}

void D3D12::CreateDevice() {
	// ファクトリーの生成
	HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory_));
	assert(SUCCEEDED(hr));

	// ハードウェアアダプタの検索
	ComPtr<IDXGIAdapter4> useAdapter;
	for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(
		i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)
	) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC3 adapterDesc;
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));
		// ソフトウェアアダプタでなければ採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			// 採用したアダプタの情報をログに出力
			Console::Print(StrUtils::ToString(std::format(L"Use Adapter : {}\n", adapterDesc.Description)));
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
	const char* featureLevelStrings[] = { "12.2", "12.1", "12.0", "11.1", "11.0" };

	// 高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		// 採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
		// 指定した機能レベルでデバイスが生成できたをできたかを確認
		if (SUCCEEDED(hr)) {
			// 生成できたのでログに出力し、ループを抜ける
			Console::Print(std::format("FeatureLevel : {}\n", featureLevelStrings[i]), kConFgColorDark);
			break;
		}
		device_ = nullptr;
	}
	assert(device_ != nullptr); // デバイスの生成がうまくいかなかったので起動できない

	device_->SetName(L"DX12Device");
}

void D3D12::SetInfoQueueBreakOnSeverity() const {
	ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
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
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
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
	if (const HRESULT hr = device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_))) {
		Console::Print(std::format("{:08x}\n", hr), kConTextColorError);
		assert(SUCCEEDED(hr));
	}
	commandQueue_->SetName(L"DX12CommandQueue");
}

void D3D12::CreateSwapChain() {
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = kFrameBufferCount;
	swapChainDesc.Width = window_->GetClientWidth();
	swapChainDesc.Height = window_->GetClientHeight();
	swapChainDesc.Format = kBufferFormat; // 色の形式
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // レンダーターゲットとして利用
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニタに映したら中身を破棄
	swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	if (window_->GetWindowHandle()) {
		swapChainDesc.Scaling = DXGI_SCALING_STRETCH; // 画面サイズに合わせて伸縮
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE; // アルファモードは無視
		Console::Print("Window Mode\n", kConFgColorDark);
	} else {
		swapChainDesc.Scaling = DXGI_SCALING_NONE; // 画面サイズに合わせない
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED; // アルファモードは事前乗算
		Console::Print("FullScreen Mode\n", kConFgColorDark);
	}

	const HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(
		commandQueue_.Get(),
		window_->GetWindowHandle(),
		&swapChainDesc,
		nullptr,
		nullptr,
		reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf())
	);

	if (hr) {
		Console::Print(std::format("{:08x}\n", hr), kConTextColorError);
		assert(SUCCEEDED(hr));
	}
}

void D3D12::CreateDescriptorHeaps() {
	// DescriptorSizeを取得しておく
	descriptorSizeRTV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// RTV用のヒープでディスクリプタの数は2。RTVはShader内で触るものではないので、ShaderVisibleはfalse
	rtvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kFrameBufferCount + kMaxRenderTargetCount, false);
	// DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので、ShaderVisibleはfalse
	dsvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, kMaxRenderTargetCount, false);
}

void D3D12::CreateRTV() {
	// いらないリソースを解放
	for (auto& rtv : renderTargets_) {
		rtv.Reset();
	}
	renderTargets_.clear();

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {}; // RTVの設定
	rtvDesc.Format = kBufferFormat; // 出力結果を書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2Dテクスチャとして書き込む

	renderTargets_.resize(kFrameBufferCount); // フレームバッファ数でリサイズ
	rtvHandles_.resize(kFrameBufferCount);

	for (unsigned int i = 0; i < kFrameBufferCount; ++i) {
		// SwapChainからResourceを引っ張ってくる
		if (const HRESULT hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&renderTargets_[i]))) {
			Console::Print(std::format("{:08x}\n", hr), kConTextColorError);
			assert(SUCCEEDED(hr));
		}

		rtvHandles_[i] = GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), descriptorSizeRTV, i);
		device_->CreateRenderTargetView(renderTargets_[i].Get(), &rtvDesc, rtvHandles_[i]);

		renderTargets_[i]->SetName(std::format(L"RenderTarget_{}", i).c_str());
	}
}

void D3D12::CreateDSV() {
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = 128; // 必要なディスクリプタ数
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dsvDescriptorHeap_));
	if (FAILED(hr)) {
		Console::Print(std::format("Failed to create DSV descriptor heap. Error code: {:08x}\n", hr), kConTextColorError);
		assert(SUCCEEDED(hr));
	}

	currentDSVIndex_ = 0;
}

void D3D12::CreateCommandAllocator() {
	const HRESULT hr = device_->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_)
	);
	if (hr) {
		Console::Print(std::format("{:08x}\n", hr), kConTextColorError);
		assert(SUCCEEDED(hr));
	}
}

void D3D12::CreateCommandList() {
	// コマンドリストの作成
	HRESULT hr = device_->CreateCommandList(
		0, // ノードマスク
		D3D12_COMMAND_LIST_TYPE_DIRECT, // コマンドリストタイプ
		commandAllocator_.Get(), // コマンドアロケーター
		nullptr, // 初期パイプラインステート
		IID_PPV_ARGS(&commandList_) // 作成されるコマンドリスト
	);

	commandList_->SetName(L"MainCommandList");

	if (FAILED(hr)) {
		Console::Print(
			std::format("Failed to create command list. Error code: {:08x}\n", hr),
			kConTextColorError
		);
		assert(SUCCEEDED(hr));
		return;
	}

	// コマンドリストを閉じる（初期状態では開いている）
	hr = commandList_->Close();
	if (FAILED(hr)) {
		Console::Print(
			std::format("Failed to close command list. Error code: {:08x}\n", hr),
			kConTextColorError
		);
		assert(SUCCEEDED(hr));
	}
}

void D3D12::CreateFence() {
	if (const HRESULT hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_))) {
		Console::Print(std::format("{:08x}\n", hr), kConTextColorError);
		assert(SUCCEEDED(hr));
	}

	fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(fenceEvent_ != nullptr);
}

void D3D12::SetViewportAndScissor() {
	// Viewport
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.Width = static_cast<FLOAT>(window_->GetClientWidth());
	viewport_.Height = static_cast<FLOAT>(window_->GetClientHeight());
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;

	// ScissorRect
	scissorRect_.left = 0;
	scissorRect_.top = 0;
	scissorRect_.right = static_cast<LONG>(window_->GetClientWidth());
	scissorRect_.bottom = static_cast<LONG>(window_->GetClientHeight());
}

void D3D12::HandleDeviceLost() {
	device_.Reset();

	// デバイスと関連リソースの再作成
	CreateDevice();
}

//-----------------------------------------------------------------------------
// Purpose: シャットダウンに備えて準備を行います
//-----------------------------------------------------------------------------
void D3D12::PrepareForShutdown() const {
	if (swapChain_) {
		BOOL isFullScreen = FALSE;
		swapChain_->GetFullscreenState(&isFullScreen, nullptr);
		if (isFullScreen) {
			// フルスクリーンモードを解除して、完了するまで待機
			swapChain_->SetFullscreenState(FALSE, nullptr);
		}
	}
}

ComPtr<ID3D12DescriptorHeap> D3D12::CreateDescriptorHeap(
	const D3D12_DESCRIPTOR_HEAP_TYPE heapType, const UINT numDescriptors, const bool shaderVisible
) const {
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	if (shaderVisible) {
		descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	} else {
		descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	}
	const HRESULT hr = device_->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	if (hr) {
		Console::Print(std::format("{:08x}\n", hr), kConTextColorError);
	}
	return descriptorHeap;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::GetCPUDescriptorHandle(
	ID3D12DescriptorHeap* descriptorHeap, const uint32_t descriptorSize, const uint32_t index
) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += static_cast<unsigned long long>(descriptorSize) * index;
	return handleCPU;
}

ComPtr<ID3D12Resource> D3D12::CreateDepthStencilTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format) const {
	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1; // Mipmapの数
	resourceDesc.DepthOrArraySize = 1; // 奥行きor配列Textureの配列数
	resourceDesc.Format = format; // DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知

	D3D12_HEAP_PROPERTIES heapProperties = {}; // 利用するHeapの設定
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // vRAM上に作る
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; // CPUのページプロパティ
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN; // メモリプールの設定

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f; // 最大値でクリア
	depthClearValue.Format = format;

	// Resourceの生成
	ComPtr<ID3D12Resource> resource;
	const HRESULT hr = device_->CreateCommittedResource(
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

RenderTargetTexture D3D12::CreateRenderTargetTexture(uint32_t width, uint32_t height, Vec4 clearColor, DXGI_FORMAT format) {
	RenderTargetTexture result = {};

	// ディスクリプタ
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = width;
	desc.Height = height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = format;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;
	clearValue.Color[0] = clearColor.x;
	clearValue.Color[1] = clearColor.y;
	clearValue.Color[2] = clearColor.z;
	clearValue.Color[3] = clearColor.w;

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	HRESULT hr = device_->CreateCommittedResource(
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
	rtvDesc.Format = format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	result.rtvHandle = AllocateNewRTVHandle();
	device_->CreateRenderTargetView(
		result.rtv.Get(),
		&rtvDesc,
		result.rtvHandle
	);

	if (srvManager_) {
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		srvDesc.Texture2D.PlaneSlice = 0;

		result.srvHandles = srvManager_->RegisterShaderResourceView(
			result.rtv.Get(), srvDesc
		);
	} else {
		Console::Print("ShaderResourceViewManager is not set.\n", kConTextColorError);
	}

	Console::Print("Created RenderTargetTexture.\n", kConTextColorCompleted, Channel::Engine);

	return result;
}

DepthStencilTexture D3D12::CreateDepthStencilTexture(uint32_t width, uint32_t height, DXGI_FORMAT format) {
	DepthStencilTexture result = {};

	// 1. デプスバッファリソース作成
	result.dsv = CreateDepthStencilTextureResource(width, height, format);

	// 2. DSVディスクリプタハンドルを算出
	uint32_t dsvIndex = currentDSVIndex_;
	++currentDSVIndex_;

	auto dsvHandle = GetCPUDescriptorHandle(
		dsvDescriptorHeap_.Get(),
		descriptorSizeDSV,
		dsvIndex
	);
	result.handles.cpuHandle = dsvHandle;

	// 3. DSVビュー作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = format;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	device_->CreateDepthStencilView(result.dsv.Get(), &dsvDesc, dsvHandle);

	return result;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::AllocateNewRTVHandle() {
	// ディスクリプタヒープのサイズを取得
	uint32_t index = static_cast<uint32_t>(rtvHandles_.size());

	// インデックスからハンドル取得
	auto handle = GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), descriptorSizeRTV, index);

	rtvHandles_.emplace_back(handle);

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
