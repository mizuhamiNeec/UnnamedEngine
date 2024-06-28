#include "Texture.h"

#include <filesystem>

#include "DirectXTex/DirectXTex.h"
#include "../Utils/ConvertString.h"
#include "DirectXTex/d3dx12.h"
#include "../Utils/Logger.h"

Texture::Texture(ID3D12Device* device, const std::wstring& filename) {
	LoadTextureFromFile(device, filename);
	CreateShaderResourceView(device);
}

Texture::~Texture() = default;

void Texture::LoadTextureFromFile(ID3D12Device* device, const std::wstring& filename) {
	ComPtr<ID3D12CommandQueue> commandQueue;
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> commandList;

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));

	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));

	DirectX::TexMetadata metadata;
	DirectX::ScratchImage scratchImage;

	HRESULT hr = LoadFromWICFile(filename.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, scratchImage);
	std::filesystem::path fullpath = std::filesystem::absolute(filename);
	Log(fullpath.generic_string());
	assert(SUCCEEDED(hr));

	DirectX::ScratchImage mipChain;
	hr = DirectX::GenerateMipMaps(
		scratchImage.GetImages(), scratchImage.GetImageCount(), scratchImage.GetMetadata(),
		DirectX::TEX_FILTER_DEFAULT, 0, mipChain
	);

	assert(SUCCEEDED(hr));

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
	textureDesc.Format = metadata.format;
	textureDesc.Width = static_cast<UINT>(metadata.width);
	textureDesc.Height = static_cast<UINT>(metadata.height);
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	textureDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&textureResource_)
	);

	assert(SUCCEEDED(hr));

	// アップロードヒープの作成
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(textureResource_.Get(), 0, static_cast<UINT>(metadata.mipLevels));

	D3D12_HEAP_PROPERTIES uploadHeapProperties = {};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC uploadBufferDesc = {};
	uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploadBufferDesc.Width = uploadBufferSize;
	uploadBufferDesc.Height = 1;
	uploadBufferDesc.DepthOrArraySize = 1;
	uploadBufferDesc.MipLevels = 1;
	uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	uploadBufferDesc.SampleDesc.Count = 1;
	uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ComPtr<ID3D12Resource> textureUploadHeap;
	hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&uploadBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&textureUploadHeap)
	);

	assert(SUCCEEDED(hr));

	// サブリソースデータの準備
	std::vector<D3D12_SUBRESOURCE_DATA> subresources(metadata.mipLevels);
	const DirectX::Image* pImages = scratchImage.GetImages();
	for (int i = 0; i < metadata.mipLevels; ++i) {
		subresources[i].pData = pImages[i].pixels;
		subresources[i].RowPitch = pImages[i].rowPitch;
		subresources[i].SlicePitch = pImages[i].slicePitch;
	}

	// テクスチャデータのアップロード
	UpdateSubresources(commandList.Get(), textureResource_.Get(), textureUploadHeap.Get(),
		0, 0, static_cast<UINT>(subresources.size()), subresources.data());

	// リソースバリアの設定
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = textureResource_.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	// コマンドリストの実行
	commandList->Close();
	ID3D12CommandList* ppCommandLists[] = {commandList.Get()};
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// GPU処理の完了を待つ
	ComPtr<ID3D12Fence> fence;
	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));
	UINT64 fenceValue = 1;
	commandQueue->Signal(fence.Get(), fenceValue);
	if (fence->GetCompletedValue() < fenceValue) {
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		fence->SetEventOnCompletion(fenceValue, eventHandle);
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void Texture::CreateShaderResourceView(ID3D12Device* device) {
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap_));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureResource_->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	device->CreateShaderResourceView(textureResource_.Get(), &srvDesc, srvHeap_->GetCPUDescriptorHandleForHeapStart());
}