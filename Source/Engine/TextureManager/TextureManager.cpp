#include "TextureManager.h"

#include <format>
#include <../Externals/DirectXTex/d3dx12.h>

#include "../Lib/Console/Console.h"
#include "../Lib/Utils/ClientProperties.h"
#include "../Lib/Utils/ConvertString.h"
#include "../Renderer/D3D12.h"

TextureManager* TextureManager::instance_ = nullptr;

/// @brief シングルトンインスタンスを取得します
/// @return TextureManagerのインスタンス
TextureManager* TextureManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new TextureManager;
	}
	return instance_;
}

/// @brief テクスチャマネージャーを初期化します
/// @param renderer D3D12レンダラーのポインタ
void TextureManager::Init(D3D12* renderer) {
	renderer_ = renderer;

	// SRVの数と同数
	//-------------------------------------------------------------------------
	// 最大枚数分のメモリ領域を確保しておく。
	// std::vectorの欠点としてあらかじめ確保していた要素数を超えたときにメモリの再割り当てが発生することが挙げられる。
	// 最初に最大個数のメモリを確保しておくことでその欠点を塞いでおく
	//-------------------------------------------------------------------------
	textureData_.reserve(kMaxSRVCount);
}

/// @brief 指定されたテクスチャのメタデータを取得します
/// @param textureIndex テクスチャのインデックス
/// @return テクスチャのメタデータ
const DirectX::TexMetadata& TextureManager::GetMetaData(const uint32_t textureIndex) const {
	// 範囲外指定違反チェック
	assert(textureIndex < textureData_.size());
	const TextureData& textureData = textureData_[textureIndex];
	return textureData.metadata;
}

/// @brief テクスチャリソースを作成します
/// @param metadata テクスチャのメタデータ
/// @return 作成されたテクスチャリソース
ComPtr<ID3D12Resource> TextureManager::CreateTextureResource(const DirectX::TexMetadata& metadata) const {
	// metadataをもとにResourceの設定
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Width = static_cast<UINT>(metadata.width); // Textureの幅
	resourceDesc.Height = static_cast<UINT>(metadata.height); // Textureの高さ
	resourceDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels); // mipMapの数
	resourceDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize); // 奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format; // TextureのFormat
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。 1固定。
	resourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension); // Textureの次元数。普段使っているのは2次元

	// 利用するHeapの設定。 非常に特殊な運用。02_04exで一般的なケース版がある
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	// Resourceの作成
	ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = renderer_->GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource)
	);
	if (FAILED(hr)) {
		Console::Print("Failed to create texture resource\n", { 1.0f, 0.0f, 0.0f, 1.0f });
		return nullptr; // エラー時はnullptrを返す
	}

	return resource;
}

/// @brief CPUディスクリプタハンドルを取得します
/// @param descriptorHeap ディスクリプタヒープ
/// @param descriptorSize ディスクリプタサイズ
/// @param index インデックス
/// @return CPUディスクリプタハンドル
D3D12_CPU_DESCRIPTOR_HANDLE TextureManager::GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap,
	const uint32_t descriptorSize,
	const uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += static_cast<unsigned long long>(descriptorSize) * index;
	return handleCPU;
}

/// @brief GPUディスクリプタハンドルを取得します
/// @param descriptorHeap ディスクリプタヒープ
/// @param descriptorSize ディスクリプタサイズ
/// @param index インデックス
/// @return GPUディスクリプタハンドル
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap,
	const uint32_t descriptorSize,
	const uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += static_cast<unsigned long long>(descriptorSize) * index;
	return handleGPU;
}

/// @brief テクスチャマネージャーを終了します
void TextureManager::Shutdown() {
	delete instance_;
	instance_ = nullptr;
}

/// @brief テクスチャデータをアップロードします
/// @param texture テクスチャリソース
/// @param mipImages MIPマップイメージ
/// @return 中間リソース
ComPtr<ID3D12Resource> TextureManager::UploadTextureData(const ComPtr<ID3D12Resource>& texture,
	const DirectX::ScratchImage& mipImages) const {
	// 中間リソースの作成
	ComPtr<ID3D12Resource> intermediateResource = nullptr;

	std::vector<D3D12_SUBRESOURCE_DATA> subResources;
	DirectX::PrepareUpload(renderer_->GetDevice(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subResources);

	const uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, static_cast<UINT>(subResources.size()));

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
	HRESULT hr = renderer_->GetDevice()->CreateCommittedResource(
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

	// サブリソースの更新
	UpdateSubresources(renderer_->GetCommandList(),
		texture.Get(),
		intermediateResource.Get(),
		0, 0,
		static_cast<UINT>(subResources.size()),
		subResources.data());

	// リソースバリアの設定
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	renderer_->GetCommandList()->ResourceBarrier(1, &barrier);

	return intermediateResource;
}

/// @brief テクスチャを読み込みます
/// @param filePath テクスチャファイルのパス
void TextureManager::LoadTexture(const std::string& filePath) {
	// 読み込み済みテクスチャを検索
	auto it = std::ranges::find_if(
		textureData_,
		[&](const TextureData& textureData) {
			return textureData.filePath == filePath;
		}
	);

	if (it != textureData_.end()) {
		return; // 読み込み済みなら早期リターン
	}

	// テクスチャ枚数上限チェック
	assert(textureData_.size() + kSRVIndexTop < kMaxSRVCount);

	// テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image = {};
	std::wstring filePathW = ConvertString::ToString(filePath);
	HRESULT hr = LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	if (FAILED(hr)) {
		Console::Print(std::format("ERROR : Failed to Load {}\n", filePath), kConsoleColorError);
		// 読み込み失敗時にデフォルトのテクスチャを読み込む
		filePathW = ConvertString::ToString("./Resources/Textures/empty.png");
		hr = LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
		assert(SUCCEEDED(hr)); // デフォルトのテクスチャも読み込めなかった場合はエラー
	}
	// MipMapの作成
	DirectX::ScratchImage mipImages = {};
	hr = GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0,
		mipImages);
	assert(SUCCEEDED(hr));

	// テクスチャデータを追加
	textureData_.resize(textureData_.size() + 1); // 要素数を1増やす
	// 追加したテクスチャデータの参照を取得する
	TextureData& textureData = textureData_.back();

	textureData.filePath = filePath;
	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = CreateTextureResource(textureData.metadata);

	ComPtr<ID3D12Resource> intermediateResource = UploadTextureData(textureData.resource, mipImages);

	//-------------------------------------------------------------------------
	// コマンドリストを閉じる
	//-------------------------------------------------------------------------
	hr = renderer_->GetCommandList()->Close();
	assert(SUCCEEDED(hr));
	if (hr) {
		Console::Print(std::format("{:08x}\n", hr), kConsoleColorError);
	}

	//-------------------------------------------------------------------------
	// コマンドのキック
	//-------------------------------------------------------------------------
	ID3D12CommandList* lists[] = { renderer_->GetCommandList() };
	renderer_->GetCommandQueue()->ExecuteCommandLists(1, lists);

	// 実行を待つ
	renderer_->WaitPreviousFrame(); // ここで実行が完了するのを待つ

	//-------------------------------------------------------------------------
	// コマンドのリセット
	//-------------------------------------------------------------------------
//	hr = renderer_->GetCommandAllocator()->Reset();
	assert(SUCCEEDED(hr));
	hr = renderer_->GetCommandList()->Reset(
		renderer_->GetCommandAllocator(),
		nullptr
	);
	assert(SUCCEEDED(hr));

	// ここまで来たら転送は終わっているので、intermediateResourceはReleaseしても良い
	intermediateResource.Reset();

	// テクスチャデータの要素数番号をSRVのインデックスとする
	uint32_t srvIndex = static_cast<uint32_t>(textureData_.size() - 1) + kSRVIndexTop; // ImGuiのために+1しておく


	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = textureData.metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = static_cast<UINT>(textureData.metadata.mipLevels);

	textureData.srvHandleCPU = GetCPUDescriptorHandle(renderer_->GetSRVDescriptorHeap(),
		renderer_->GetDevice()->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV), srvIndex);
	textureData.srvHandleGPU = GetGPUDescriptorHandle(renderer_->GetSRVDescriptorHeap(),
		renderer_->GetDevice()->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV), srvIndex);

	renderer_->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, textureData.srvHandleCPU);
}

/// @brief ファイルパスからテクスチャのインデックスを取得します
/// @param filePath テクスチャファイルのパス
/// @return テクスチャのインデックス
uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath) {
	// 読み込み済みテクスチャを検索
	const auto it = std::ranges::find_if(
		textureData_,
		[&](const TextureData& textureData) {
			return textureData.filePath == filePath;
		}
	);

	if (it != textureData_.end()) {
		// 読み込み済みなら要素番号を返す
		const uint32_t textureIndex = static_cast<uint32_t>(std::distance(textureData_.begin(), it));
		return textureIndex;
	}

	assert(0);
	return 0;
}

/// @brief テクスチャのGPUディスクリプタハンドルを取得します
/// @param textureIndex テクスチャのインデックス
/// @return GPUディスクリプタハンドル
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const uint32_t textureIndex) {
	if (textureIndex >= textureData_.size()) {
		Console::Print("範囲外指定違反: textureIndexが無効です。", { 1.0f, 0.0f, 0.0f, 1.0f });
		return {};
	}
	TextureData& textureData = textureData_[textureIndex];
	return textureData.srvHandleGPU;
}
