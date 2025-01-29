#include "Texture.h"

#include <DirectXTex/d3dx12.h>
#include <DirectXTex/DirectXTex.h>
#include <SubSystem/Console/Console.h>
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
		Console::Print(std::format("テクスチャの読み込みに失敗しました: {}\n", filePath), kConsoleColorError, Channel::ResourceSystem);
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
	//metadata_ = image.GetMetadata();
	textureResource_ = CreateTextureResource(d3d12->GetDevice());
	textureResource_->SetName(StrUtils::ToWString(filePath).c_str());

	ComPtr<ID3D12Resource> intermediateResource = UploadTextureData(d3d12->GetDevice(), d3d12->GetCommandList(), textureResource_, mipImages);
	//ComPtr<ID3D12Resource> intermediateResource = UploadTextureData(d3d12->GetDevice(), d3d12->GetCommandList(), textureResource_, image);
	intermediateResource->SetName(StrUtils::ToWString(filePath).c_str());

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

	Console::Print(std::format("テクスチャを読み込みました: {}\n", filePath), kConsoleColorCompleted, Channel::ResourceSystem);

	return true;
}

D3D12_GPU_DESCRIPTOR_HANDLE Texture::GetShaderResourceView() const { return handle_; }

bool Texture::CreateErrorTexture(D3D12* d3d12, ShaderResourceViewManager* shaderResourceViewManager) {
	// デバッグ時は黒ピンクチェッカーの作成
	constexpr uint32_t kCheckerSize = 32;
	uint32_t checkerData[kCheckerSize * kCheckerSize] = {};

	for (uint32_t y = 0; y < kCheckerSize; ++y) {
		for (uint32_t x = 0; x < kCheckerSize; ++x) {
			if ((x / 4 + y / 4) % 2 == 0) {
				constexpr uint32_t kPink = 0xffff00ff;
				checkerData[y * kCheckerSize + x] = kPink;
			} else {
				constexpr uint32_t kBlack = 0xFF000000;
				checkerData[y * kCheckerSize + x] = kBlack;
			}
		}
	}

	// メタデータの設定
	metadata_.width = kCheckerSize;
	metadata_.height = kCheckerSize;
	metadata_.format = DXGI_FORMAT_R8G8B8A8_UNORM;
	metadata_.arraySize = 1;
	metadata_.mipLevels = 1;
	metadata_.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;

	// テクスチャリソースの作成
	textureResource_ = CreateTextureResource(d3d12->GetDevice());
	textureResource_->SetName(L"ErrorTexture");
	if (!textureResource_) {
		return false;
	}

	// テクスチャのアップロード
	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = checkerData;
	textureData.RowPitch = kCheckerSize * sizeof(uint32_t);
	textureData.SlicePitch = textureData.RowPitch * kCheckerSize;

	ComPtr<ID3D12Resource> intermediateResource = UploadTextureData(
		d3d12->GetDevice(),
		d3d12->GetCommandList(),
		textureResource_,
		textureData
	);
	intermediateResource->SetName(L"ErrorTexture");

	if (!intermediateResource) {
		return false;
	}

	// コマンドの実行と待機
	d3d12->GetCommandList()->Close();
	ID3D12CommandList* commandLists[] = { d3d12->GetCommandList() };
	d3d12->GetCommandQueue()->ExecuteCommandLists(1, commandLists);
	d3d12->WaitPreviousFrame();
	d3d12->GetCommandList()->Reset(d3d12->GetCommandAllocator(), nullptr);

	// SRVの作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = metadata_.format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	handle_ = shaderResourceViewManager->RegisterShaderResourceView(
		textureResource_.Get(),
		srvDesc
	);

	return true;
}

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
	resource->SetName(L"TextureResource");

	if (FAILED(hr)) {
		Console::Print("テクスチャリソースの作成に失敗しました。\n", kConsoleColorError, Channel::ResourceSystem);
		assert(SUCCEEDED(hr));
	}

	return resource;
}

ComPtr<ID3D12Resource> Texture::UploadTextureData(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages) {
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
		Console::Print("アップロード用リソースの作成に失敗しました。\n", kConsoleColorError, Channel::ResourceSystem);
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

ComPtr<ID3D12Resource> Texture::UploadTextureData(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList,
	const ComPtr<ID3D12Resource>& texture,
	const D3D12_SUBRESOURCE_DATA& textureData
) {
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(
		texture.Get(), 0, 1
	);

	// アップロードヒープの作成
	D3D12_HEAP_PROPERTIES uploadHeapProperties = {};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC uploadResourceDesc = {};
	uploadResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploadResourceDesc.Width = uploadBufferSize;
	uploadResourceDesc.Height = 1;
	uploadResourceDesc.DepthOrArraySize = 1;
	uploadResourceDesc.MipLevels = 1;
	uploadResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	uploadResourceDesc.SampleDesc.Count = 1;
	uploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ComPtr<ID3D12Resource> intermediateResource;
	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&uploadResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&intermediateResource)
	);

	if (FAILED(hr)) {
		return nullptr;
	}

	UpdateSubresources(
		commandList,
		texture.Get(),
		intermediateResource.Get(),
		0, 0, 1,
		&textureData
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
