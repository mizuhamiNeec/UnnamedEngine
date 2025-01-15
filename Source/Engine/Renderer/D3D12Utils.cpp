#include "D3D12Utils.h"

#include "DirectXTex/d3dx12.h"
#include "Lib/Console/Console.h"

ComPtr<ID3D12Resource> D3D12Utils::CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata) {
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension); // Textureの次元数。普段使っているのは2次元
	resourceDesc.Alignment = 0;
	resourceDesc.Width = metadata.width; // Textureの幅
	resourceDesc.Height = static_cast<UINT>(metadata.height); // Textureの高さ
	resourceDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize); // 奥行き or 配列Textureの配列数
	resourceDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels); // mipMapの数
	resourceDesc.Format = metadata.format; // TextureのFormat
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。 1固定。
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	// 利用するHeapの設定。 非常に特殊な運用。02_04exで一般的なケース版がある
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // デフォルトのヒープを使用

	// リソースの作成
	ComPtr<ID3D12Resource> resource;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource)
	);

	if (FAILED(hr)) {
		Console::Print("テクスチャリソースの作成に失敗しました。\n", kConsoleColorError);
		return nullptr; // エラー時はnullptrを返す
	}

	return resource;
}

ComPtr<ID3D12Resource> D3D12Utils::UploadTextureData(
	ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const ComPtr<ID3D12Resource>& textureResource,
	const DirectX::ScratchImage& mipImages
) {
	// 中間リソースの作成
	ComPtr<ID3D12Resource> intermediateResource = nullptr;

	std::vector<D3D12_SUBRESOURCE_DATA> subResources;
	HRESULT hr = PrepareUpload(
		device, mipImages.GetImages(), mipImages.GetImageCount(),
		mipImages.GetMetadata(), subResources
	);

	if (FAILED(hr)) {
		Console::Print("アップロード用のデータの準備に失敗しました。\n", kConsoleColorError);
		return nullptr;
	}

	const uint64_t intermediateSize = GetRequiredIntermediateSize(
		textureResource.Get(), 0,
		static_cast<UINT>(subResources.size())
	);

	// アップロード用のヒーププロパティ
	D3D12_HEAP_PROPERTIES uploadHeapProperties = {};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	// リソースの設定
	D3D12_RESOURCE_DESC uploadResourceDesc = {};
	uploadResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploadResourceDesc.Width = intermediateSize;
	uploadResourceDesc.Height = 1;
	uploadResourceDesc.DepthOrArraySize = 1;
	uploadResourceDesc.MipLevels = 1;
	uploadResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	uploadResourceDesc.SampleDesc.Count = 1;
	uploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 中間リソースの作成
	hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&uploadResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&intermediateResource)
	);

	if (FAILED(hr)) {
		Console::Print("中間リソースの作成に失敗しました。\n", kConsoleColorError);
		return nullptr;
	}

	// サブリソースの更新
	UpdateSubresources(
		commandList,
		textureResource.Get(),
		intermediateResource.Get(),
		0, 0,
		static_cast<UINT>(subResources.size()),
		subResources.data()
	);

	// リソースバリアの設定
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = textureResource.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier(1, &barrier);

	return intermediateResource;
}

void D3D12Utils::CreateSRVForTexture2D(
	ID3D12Device* device, ID3D12Resource* resource, const DirectX::TexMetadata& metadata,
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle
) {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = metadata.format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	// SRVを作成
	device->CreateShaderResourceView(resource, &srvDesc, srvHandle);
}

void D3D12Utils::CreateSRVForStructuredBuffer(
	ID3D12Device* device, ID3D12Resource* resource, UINT numElements, UINT structureByteStride,
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle
) {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = numElements;
	srvDesc.Buffer.StructureByteStride = structureByteStride;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;

	// SRVを作成
	device->CreateShaderResourceView(resource, &srvDesc, srvHandle);
}
