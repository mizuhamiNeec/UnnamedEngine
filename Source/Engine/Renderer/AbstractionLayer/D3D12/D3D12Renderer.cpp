#include "D3D12Renderer.h"

#include <cassert>

#include <Renderer/AbstractionLayer/D3D12/D3D12Device.h>
#include <Renderer/AbstractionLayer/Base/BaseDevice.h>

#include "Lib/Utils/StrUtils.h"

#include "Renderer/D3D12.h"

#include "SubSystem/Console/Console.h"

D3D12Renderer::D3D12Renderer() : d3d12Device_(nullptr) {}

D3D12Renderer::~D3D12Renderer() {
	D3D12Renderer::Shutdown();
}

bool D3D12Renderer::Init(const RendererInitInfo& info) {
	if (info.enableDebugLayer) {
		ID3D12Debug6* debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
			debugController->EnableDebugLayer(); // デバッグレイヤーの有効化
			debugController->SetEnableGPUBasedValidation(TRUE); // GPU側でのチェックを有効化
			debugController->Release();
		}
	}

	CreateDevice();

	if (info.enableValidationLayer) {
		ComPtr<ID3D12InfoQueue> infoQueue;
		if (SUCCEEDED(device_.As(&infoQueue))) {
			// やばいエラー時に止まる
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			// エラー時に止まる
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			// 警告時に止まる
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_INFO, TRUE);

			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_MESSAGE, TRUE);
			infoQueue->Release();

			// 抑制するメッセージのID
			std::vector<D3D12_MESSAGE_ID> denyIds = {
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
	}

	d3d12Device_ = std::make_unique<D3D12Device>();
	return d3d12Device_ != nullptr;
}

void D3D12Renderer::Shutdown() {
}

BaseDevice* D3D12Renderer::GetDevice() {
	return d3d12Device_.get();
}

void D3D12Renderer::CreateDevice() {
	// デバイスの作成
	HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory_));
	assert(SUCCEEDED(hr));

	// ハードウェアアダプタの検索
	ComPtr<IDXGIAdapter4> useAdapter;
	for (
		UINT i = 0;
		dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND;
		++i
		) {
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
	const std::vector featureLevels = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	std::vector<const char*> featureLevelStrings = { "12.2", "12.1", "12.0", "11.1", "11.0" };

	// 高い順に生成できるか試していく
	for (size_t i = 0; i < featureLevels.size(); ++i) {
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
}
