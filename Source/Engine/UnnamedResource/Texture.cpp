#include "Texture.h"

#include <DirectXTex/d3dx12.h>
#include <DirectXTex/DirectXTex.h>
#include <Lib/Console/Console.h>
#include <Lib/Utils/StrUtils.h>

bool Texture::LoadFromFile(
	D3D12* d3d12,
	ShaderResourceViewManager* shaderResourceViewManager,
	const std::string& filePath
) {
	// TODO: 読み込み済みテクスチャを検索

	// TODO: テクスチャ枚数上限チェック

	// テクスチャファイルを読み込んでプログラムで扱えるようにする
	DirectX::ScratchImage image = {};
	std::wstring filePathW = StrUtils::ToWString(filePath);
	// TODO: 要検証
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	// 失敗した場合
	if (FAILED(hr)) {
		Console::Print("テクスチャの読み込みに失敗しました。\n", kConsoleColorError, Channel::ResourceManager);
		return false;
	}

	// MipMapの作成 TODO: 本当に必要かどうか要検証
	DirectX::ScratchImage mipImages = {};
	hr = GenerateMipMaps(
		image.GetImages(),
		image.GetImageCount(),
		image.GetMetadata(),
		DirectX::TEX_FILTER_SRGB,
		0,
		mipImages
	);

	assert(SUCCEEDED(hr));

	metadata_ = mipImages.GetMetadata();
	textureResource_ = CreateTextureResource(d3d12->GetDevice());

	ComPtr<ID3D12Resource> intermediateResource = UploadTextureData(d3d12->GetDevice(), d3d12->GetCommandList(), textureResource_, mipImages);

	// コマンドリストを閉じる
	hr = d3d12->GetCommandList()->Close();
	assert(SUCCEEDED(hr));

	// コマンドのキック
	ID3D12CommandList* commandLists[] = { d3d12->GetCommandList() };
	d3d12->GetCommandQueue()->ExecuteCommandLists(1, commandLists);

	// 実行を待つ
	d3d12->WaitPreviousFrame();

	hr = d3d12->GetCommandList()->Reset(
		d3d12->GetCommandAllocator(), nullptr
	);
	assert(SUCCEEDED(hr));

	// 中間リソースの解放
	intermediateResource.Reset();

	// SRVを登録
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = metadata_.format; // フォーマット
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata_.mipLevels); // MipMapの数

	handle_ = shaderResourceViewManager->RegisterShaderResourceView(textureResource_.Get(), srvDesc);

	return true;
}

D3D12_GPU_DESCRIPTOR_HANDLE Texture::GetShaderResourceView() const { return handle_; }

ComPtr<ID3D12Resource> Texture::CreateTextureResource(ID3D12Device* device) {
	// metadataをもとにResourceの設定
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Width = static_cast<UINT>(metadata_.width); // Textureの幅
	resourceDesc.Height = static_cast<UINT>(metadata_.height); // Textureの高さ
	resourceDesc.MipLevels = static_cast<UINT16>(metadata_.mipLevels); // MipMapの数
	resourceDesc.DepthOrArraySize = static_cast<UINT16>(metadata_.arraySize); // 奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata_.format; // TextureのFormat
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定。
	resourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata_.dimension); // Textureの次元数。普段使っているのは2次元

	// 利用するHeapの設定。非常に特殊な運用。02_04exで一般的なケース版がある
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	// Resourceの作成
	ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource)
	);

	if (FAILED(hr)) {
		Console::Print("テクスチャリソースの作成に失敗しました。\n", kConsoleColorError, Channel::ResourceManager);
		assert(SUCCEEDED(hr));
	}

	return resource;
}

ComPtr<ID3D12Resource> Texture::UploadTextureData(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages) const {
	// 中間リソースの作成
	ComPtr<ID3D12Resource> intermediateResource = nullptr;

	std::vector<D3D12_SUBRESOURCE_DATA> subResources;
	DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subResources);

	const UINT64 intermediateSize = GetRequiredIntermediateSize(
		texture.Get(),
		0,
		static_cast<UINT>(subResources.size())
	);

	// アップロード用のヒーププロパティ
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

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
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&uploadResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&intermediateResource)
	);

	if (FAILED(hr)) {
		Console::Print("アップロード用リソースの作成に失敗しました。\n", kConsoleColorError, Channel::ResourceManager);
		return nullptr;
	}

	// サブリソースの更新
	UpdateSubresources(
		commandList,
		texture.Get(),                   // デスティネーションリソース（テクスチャリソース）
		intermediateResource.Get(),      // アップロード用中間リソース
		0,
		0,
		static_cast<UINT>(subResources.size()),
		subResources.data()
	);

	// リソースバリアの設定
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(1, &barrier);

	return intermediateResource;
}
