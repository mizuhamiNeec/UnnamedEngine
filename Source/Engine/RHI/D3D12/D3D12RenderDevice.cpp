#include "D3D12RenderDevice.h"

#include "Lib/Utils/StrUtils.h"

#include "SubSystem/Console/Console.h"

bool D3D12RenderDevice::Init() {
#ifdef _DEBUG
	EnableDebugLayer();
#endif

	// ファクトリの作成
	ComPtr<IDXGIFactory7> factory;
	HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
	if (FAILED(hr)) {
		Console::Print("Failed to create DXGI Factory.", kConTextColorError, Channel::RenderSystem);
		return false;
	}

	// ハードウェアアダプタの検索
	ComPtr<IDXGIAdapter4> useAdapter;
	for (
		UINT i = 0;
		factory->EnumAdapterByGpuPreference(
			i,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND;
		++i) {
		DXGI_ADAPTER_DESC3 adapterDesc;
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));
		// ソフトウェアアダプタでなければ採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			Console::Print(
				StrUtils::ToString(std::format(L"Use Adapter : {}\n", adapterDesc.Description)),
				kConTextColorCompleted,
				Channel::RenderSystem);
			break;
		}
		useAdapter = nullptr; // ソフトウェアアダプタの場合は見なかったことにする
	}
	assert(useAdapter != nullptr); // 採用したアダプタが見つからなかったので起動できない

	// デバイスの生成
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	const std::vector<std::string> featureLevelStrings = {
		"12.2",
		"12.1",
		"12.0",
		"11.1",
		"11.0"
	};

	// 高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		// 採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
		// 指定した機能レベルでデバイスが生成できたをできたかを確認
		if (SUCCEEDED(hr)) {
			// 生成できたのでログに出力し、ループを抜ける
			Console::Print(
				"FeatureLevel: " + featureLevelStrings[i] + "\n",
				kConTextColorCompleted,
				Channel::RenderSystem
			);
			break;
		}
		device_ = nullptr;
	}
	assert(device_ != nullptr); // デバイスの生成がうまくいかなかったので起動できない

	ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		// やばいエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		// エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		// 警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
		// 抑制するメッセージのID
		std::vector denyIds = {
			// Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
			// https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		// 抑制するレベル
		std::vector severities = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = static_cast<UINT>(denyIds.size());
		filter.DenyList.pIDList = denyIds.data();
		filter.DenyList.NumSeverities = static_cast<UINT>(severities.size());
		filter.DenyList.pSeverityList = severities.data();
		// 指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);
	}

	// コマンドキューの生成
	constexpr D3D12_COMMAND_QUEUE_DESC queueDesc = {
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0,
	};
	hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_));
	if (FAILED(hr)) {
		Console::Print(std::format("{:08x}\n", hr), kConTextColorError, Channel::RenderSystem);
		assert(SUCCEEDED(hr));
		return false;
	}

	return false;
}

bool D3D12RenderDevice::Shutdown() {
	return false;
}

void D3D12RenderDevice::EnableDebugLayer() {
	ComPtr<ID3D12Debug6> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();				// デバッグレイヤーの有効化
		debugController->SetEnableGPUBasedValidation(TRUE); // GPU側でのチェックを有効化
		debugController->Release();							// デバッグコントローラの解放
	}
}
