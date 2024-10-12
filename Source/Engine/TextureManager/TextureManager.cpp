#include "TextureManager.h"
#include <format>

#include "../Utils/ConvertString.h"
#include "../../../Console.h"

TextureManager* TextureManager::instance = nullptr;


// HACK : 要修正

// TODO : シングルトンは悪!!
TextureManager* TextureManager::GetInstance() {
	if (instance == nullptr) {
		instance = new TextureManager;
	}
	return instance;
}

std::shared_ptr<Texture> TextureManager::GetTexture(ID3D12Device* device, const std::wstring& filename) {
	auto it = textures.find(filename);
	if (it != textures.end()) {
		return it->second;
	}

	std::shared_ptr<Texture> texture = std::make_shared<Texture>(device, filename);
	textures[filename] = texture;
	return texture;
}

void TextureManager::Initialize(const ComPtr<ID3D12Device>& device) {
	device_ = device;

	// SRVの数と同数
	//-------------------------------------------------------------------------
	// 最大枚数分のメモリ領域を確保しておく。
	// std::vectorの欠点としてあらかじめ確保していた要素数を超えたときにメモリの再割り当てが発生することが挙げられる。
	// 最初に最大個数のメモリを確保しておくことでその欠点を塞いでおく
	//-------------------------------------------------------------------------
	textureData_.reserve(kMaxSRVCount);
}

void TextureManager::ReleaseUnusedTextures() {
	for (auto it = textures.begin(); it != textures.end(); ) {
		if (it->second.use_count() == 1) {
			it = textures.erase(it);
		} else {
			++it;
		}
	}
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
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし。
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_GENERIC_READ, // 初回のResourceState。Textureは基本読むだけ
		nullptr,
		IID_PPV_ARGS(&resource) // 作成するResourceポインタへのポインタ
	);
	assert(SUCCEEDED(hr));
	Console::Print(std::format("{:08x}\n", hr));

	return resource;
}

D3D12_CPU_DESCRIPTOR_HANDLE TextureManager::GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, const uint32_t descriptorSize, const uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += static_cast<unsigned long long>(descriptorSize) * index;
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, const uint32_t descriptorSize, const uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += static_cast<unsigned long long>(descriptorSize) * index;
	return handleGPU;
}

void TextureManager::Shutdown() {
	delete instance;
	instance = nullptr;

	for (auto& pair : textures) {
		if (pair.second.use_count() > 1) {
			// テクスチャがまだ参照されている
			Console::Print(ConvertString::ToString(std::format(L"Texture {} is still in use, use_count: {}\n", pair.first.c_str(), pair.second.use_count())));
		}
	}
	textures.clear();
}

void TextureManager::LoadTexture(const std::string& filePath) {
	// テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image = {};
	std::wstring filePathW = ConvertString::ToString(filePath);
	HRESULT hr = LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	// MipMapの作成
	DirectX::ScratchImage mipImages = {};
	hr = GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	// テクスチャデータを追加
	textureData_.resize(textureData_.size() + 1); // 要素数を1増やす
	// 追加したテクスチャデータの参照を取得する
	TextureData& textureData = textureData_.back();

	// テクスチャデータの要素数番号をSRVのインデックスとする
	uint32_t srvIndex = static_cast<uint32_t>(textureData_.size());

	textureData = {
		.filePath = filePath,
		.metadata = mipImages.GetMetadata(),
		.resource = CreateTextureResource(textureData.metadata),
		.srvHandleCPU =
	};


}
