#include "TextureManager.h"

#include <format>

#include <../Externals/DirectXTex/d3dx12.h>

#include "../Lib/Utils/ClientProperties.h"
#include "../Lib/Utils/ConvertString.h"
#include "../Renderer/D3D12.h"
#include "../Lib/Console/Console.h"

TextureManager* TextureManager::instance_ = nullptr;

// HACK : 要修正
// TODO : シングルトンは悪!!
TextureManager* TextureManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new TextureManager;
	}
	return instance_;
}

void TextureManager::Initialize(D3D12* renderer) {
	renderer_ = renderer;

	// SRVの数と同数
	//-------------------------------------------------------------------------
	// 最大枚数分のメモリ領域を確保しておく。
	// std::vectorの欠点としてあらかじめ確保していた要素数を超えたときにメモリの再割り当てが発生することが挙げられる。
	// 最初に最大個数のメモリを確保しておくことでその欠点を塞いでおく
	//-------------------------------------------------------------------------
	textureData_.reserve(kMaxSRVCount);
}

const DirectX::TexMetadata& TextureManager::GetMetaData(const uint32_t textureIndex) const {
	// 範囲外指定違反チェック
	assert(textureIndex < textureData_.size());
	const TextureData& textureData = textureData_[textureIndex];
	return textureData.metadata;
}

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
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM; // 細かい設定を行う
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; // WriteBackポリシーでCPUアクセス可能
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0; // プロセッサの近くに配置

	// Resourceの生成
	ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = renderer_->GetDevice()->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし。
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_GENERIC_READ, // 初回のResourceState。Textureは基本読むだけ
		nullptr,
		IID_PPV_ARGS(&resource) // 作成するResourceポインタへのポインタ
	);
	if (FAILED(hr)) {
		Console::Print("Failed to create texture resource\n", { 1.0f, 0.0f, 0.0f, 1.0f });
		return nullptr; // エラー時はnullptrを返す
	}

	return resource;
}

D3D12_CPU_DESCRIPTOR_HANDLE TextureManager::GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap,
	const uint32_t descriptorSize,
	const uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += static_cast<unsigned long long>(descriptorSize) * index;
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap,
	const uint32_t descriptorSize,
	const uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += static_cast<unsigned long long>(descriptorSize) * index;
	return handleGPU;
}

void TextureManager::Shutdown() {
	delete instance_;
	instance_ = nullptr;
}

void TextureManager::UploadTextureData(const ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages) {
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

	// 全MipMapについて
	for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
		// MipMapLevelを指定して各Imageを取得
		const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);
		// Textureに転送
		HRESULT hr = texture->WriteToSubresource(
			static_cast<UINT>(mipLevel),
			nullptr, // 全領域へコピー
			img->pixels, // 元データアドレス
			static_cast<UINT>(img->rowPitch), // 1ラインサイズ
			static_cast<UINT>(img->slicePitch) // 1枚サイズ
		);
		if (FAILED(hr)) {
			Console::Print("Failed to upload texture data to subresource\n", { 1.0f, 0.0f, 0.0f, 1.0f });
		}
	}
}

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
	assert(SUCCEEDED(hr));

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
	UploadTextureData(textureData.resource, mipImages);

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
		uint32_t textureIndex = static_cast<uint32_t>(std::distance(textureData_.begin(), it));
		return textureIndex;
	}

	assert(0);
	return 0;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const uint32_t textureIndex) {
	if (textureIndex >= textureData_.size()) {
		Console::Print("範囲外指定違反: textureIndexが無効です。", { 1.0f, 0.0f, 0.0f, 1.0f });
		return {};
	}

	TextureData& textureData = textureData_[textureIndex];
	return textureData.srvHandleGPU;
}
