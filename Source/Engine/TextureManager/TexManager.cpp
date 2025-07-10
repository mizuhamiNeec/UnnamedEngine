#include "TexManager.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <filesystem>
#include <format>
#include <../Externals/DirectXTex/d3dx12.h>

#include "../Lib/Utils/ClientProperties.h"
#include "../Renderer/D3D12.h"
#include "SrvManager.h"

#include "Lib/Utils/StrUtil.h"

#include "SubSystem/Console/Console.h"


TexManager* TexManager::instance_ = nullptr;

/// @brief シングルトンインスタンスを取得します
/// @return TexManagerのインスタンス
TexManager* TexManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new TexManager;
	}
	return instance_;
}

/// @brief テクスチャマネージャーを初期化します
/// @param renderer D3D12レンダラーのポインタ
/// @param srvManager Srvマネージャのポインタ
void TexManager::Init(D3D12* renderer, SrvManager* srvManager) {
	renderer_   = renderer;
	srvManager_ = srvManager;

	// SRVの数と同数
	//-------------------------------------------------------------------------
	// 最大枚数分のメモリ領域を確保しておく。
	// std::vectorの欠点としてあらかじめ確保していた要素数を超えたときにメモリの再割り当てが発生することが挙げられる。
	// 最初に最大個数のメモリを確保しておくことでその欠点を塞いでおく
	//-------------------------------------------------------------------------
	textureData_.reserve(kMaxSrvCount);
}

/// @brief 指定されたテクスチャのメタデータを取得します
/// @param filePath
/// @return テクスチャのメタデータ
const DirectX::TexMetadata& TexManager::GetMetaData(
	const std::string& filePath) const {
	auto it = textureData_.find(filePath);
	assert(it != textureData_.end()); // ファイルが存在することを確認
	const TextureData& textureData = it->second;
	return textureData.metadata;
}

/// @brief テクスチャリソースを作成します
/// @param metadata テクスチャのメタデータ
/// @return 作成されたテクスチャリソース
ComPtr<ID3D12Resource> TexManager::CreateTextureResource(
	const DirectX::TexMetadata& metadata) const {
	// metadataをもとにResourceの設定
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Width = static_cast<UINT>(metadata.width); // Textureの幅
	resourceDesc.Height = static_cast<UINT>(metadata.height); // Textureの高さ
	resourceDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
	// mipMapの数
	resourceDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
	// 奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format; // TextureのFormat
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。 1固定。
	resourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.
		dimension); // Textureの次元数。普段使っているのは2次元

	// 利用するHeapの設定。 非常に特殊な運用。02_04exで一般的なケース版がある
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type                  = D3D12_HEAP_TYPE_DEFAULT;
	// Resourceの作成
	ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT                hr = renderer_->GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource)
	);

	if (FAILED(hr)) {
		Console::Print("Failed to create texture resource\n",
		               {1.0f, 0.0f, 0.0f, 1.0f});
		return nullptr; // エラー時はnullptrを返す
	}

	return resource;
}

/// @brief CPUディスクリプタハンドルを取得します
/// @param descriptorHeap ディスクリプタヒープ
/// @param descriptorSize ディスクリプタサイズ
/// @param index インデックス
/// @return CPUディスクリプタハンドル
D3D12_CPU_DESCRIPTOR_HANDLE TexManager::GetCPUDescriptorHandle(
	ID3D12DescriptorHeap* descriptorHeap,
	const uint32_t        descriptorSize,
	const uint32_t        index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->
		GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += static_cast<unsigned long long>(descriptorSize) * index;
	return handleCPU;
}

/// @brief GPUディスクリプタハンドルを取得します
/// @param descriptorHeap ディスクリプタヒープ
/// @param descriptorSize ディスクリプタサイズ
/// @param index インデックス
/// @return GPUディスクリプタハンドル
D3D12_GPU_DESCRIPTOR_HANDLE TexManager::GetGPUDescriptorHandle(
	ID3D12DescriptorHeap* descriptorHeap,
	const uint32_t        descriptorSize,
	const uint32_t        index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->
		GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += static_cast<unsigned long long>(descriptorSize) * index;
	return handleGPU;
}

/// @brief テクスチャマネージャーを終了します
void TexManager::Shutdown() {
	delete instance_;
	instance_ = nullptr;
}

/// @brief テクスチャデータをアップロードします
/// @param texture テクスチャリソース
/// @param mipImages MIPマップイメージ
/// @return 中間リソース
ComPtr<ID3D12Resource> TexManager::UploadTextureData(
	const ComPtr<ID3D12Resource>& texture,
	const DirectX::ScratchImage&  mipImages) const {
	// 中間リソースの作成
	ComPtr<ID3D12Resource> intermediateResource = nullptr;

	std::vector<D3D12_SUBRESOURCE_DATA> subResources;
	DirectX::PrepareUpload(renderer_->GetDevice(), mipImages.GetImages(),
	                       mipImages.GetImageCount(),
	                       mipImages.GetMetadata(), subResources);

	const uint64_t intermediateSize = GetRequiredIntermediateSize(
		texture.Get(), 0,
		static_cast<UINT>(subResources.size()));

	// アップロード用のヒーププロパティ
	D3D12_HEAP_PROPERTIES uploadHeapProperties = {};
	uploadHeapProperties.Type                  = D3D12_HEAP_TYPE_UPLOAD;

	// リソースの設定
	D3D12_RESOURCE_DESC uploadResourceDesc = {};
	uploadResourceDesc.Dimension           = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploadResourceDesc.Width               = intermediateSize;
	uploadResourceDesc.Height              = 1;
	uploadResourceDesc.DepthOrArraySize    = 1;
	uploadResourceDesc.MipLevels           = 1;
	uploadResourceDesc.Format              = DXGI_FORMAT_UNKNOWN;
	uploadResourceDesc.SampleDesc.Count    = 1;
	uploadResourceDesc.Layout              = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

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
	barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource   = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_GENERIC_READ;
	renderer_->GetCommandList()->ResourceBarrier(1, &barrier);

	return intermediateResource;
}

/// @brief テクスチャを読み込みます
/// @param filePath テクスチャファイルのパス
void TexManager::LoadTexture(const std::string& filePath, bool forceCubeMap) {
	// 読み込み済みテクスチャを検索
	if (textureData_.contains(filePath)) {
		Console::Print(std::format("LoadTexture: {} は既に読み込み済みです\n", filePath));
		return; // 読み込み済みなら早期リターン
	}

	// テクスチャ枚数上限チェック
	assert(srvManager_->CanAllocate());

	// ファイル拡張子を取得して小文字に変換
	std::string extension = filePath.substr(filePath.find_last_of('.') + 1);
	// 小文字に変換
	DirectX::ScratchImage image     = {};
	std::wstring          filePathW = StrUtil::ToWString(filePath);
	HRESULT               hr        = E_FAIL;

	bool isCubeMap = forceCubeMap;

	// 拡張子に応じた読み込み処理
	if (extension == "dds") {
		// DDSファイルの読み込み
		DirectX::TexMetadata metadata;
		hr = DirectX::GetMetadataFromDDSFile(filePathW.c_str(),
		                                     DirectX::DDS_FLAGS_NONE, metadata);

		if (SUCCEEDED(hr)) {
			// キューブマップかどうかをチェック
			isCubeMap = isCubeMap || (metadata.miscFlags &
				DirectX::TEX_MISC_TEXTURECUBE);

			// メタデータを取得したら改めて読み込み
			hr = DirectX::LoadFromDDSFile(filePathW.c_str(),
			                              DirectX::DDS_FLAGS_NONE, nullptr,
			                              image);
		}
	} else {
		// WICファイル（PNG, JPG, BMPなど）の読み込み
		hr = DirectX::LoadFromWICFile(filePathW.c_str(),
		                              DirectX::WIC_FLAGS_FORCE_SRGB, nullptr,
		                              image);
	}

	// 読み込み失敗時の処理
	if (FAILED(hr)) {
		Console::Print(std::format("ERROR : Failed to Load {}\n", filePath));
		// デフォルトテクスチャの読み込み
		filePathW = StrUtil::ToWString("./Resources/Textures/empty.png");
		hr        = DirectX::LoadFromWICFile(filePathW.c_str(),
		                                     DirectX::WIC_FLAGS_FORCE_SRGB,
		                                     nullptr,
		                                     image);
		assert(SUCCEEDED(hr)); // デフォルトのテクスチャも読み込めなかった場合はエラー
		isCubeMap = false;     // デフォルトテクスチャはキューブマップではない
	}

	DirectX::ScratchImage mipImages = {};

	// DDSファイルで既にミップマップがある場合は生成しない
	if (extension == "dds" && image.GetMetadata().mipLevels > 1) {
		// 既存のミップマップを使用
		mipImages = std::move(image);
	} else {
		// ミップマップの生成
		hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(),
		                              image.GetMetadata(),
		                              DirectX::TEX_FILTER_SRGB, 0,
		                              mipImages);
		assert(SUCCEEDED(hr));
	}

	// テクスチャデータを追加
	TextureData& textureData = textureData_[filePath];

	textureData.filePath = filePath;
	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = CreateTextureResource(textureData.metadata);

	ComPtr<ID3D12Resource> intermediateResource = UploadTextureData(
		textureData.resource, mipImages);

	// コマンドリストを閉じる
	hr = renderer_->GetCommandList()->Close();
	assert(SUCCEEDED(hr));
	if (hr) {
		Console::Print(std::format("{:08x}\n", hr));
	}

	// コマンドのキック
	ID3D12CommandList* lists[] = {renderer_->GetCommandList()};
	renderer_->GetCommandQueue()->ExecuteCommandLists(1, lists);

	// 実行を待つ
	renderer_->WaitPreviousFrame();

	// コマンドのリセット
	hr = renderer_->GetCommandList()->Reset(
		renderer_->GetCommandAllocator(),
		nullptr
	);
	assert(SUCCEEDED(hr));

	// リソース解放
	intermediateResource.Reset();

	// SRV確保（テクスチャ用専用Allocateを使用）
	textureData.srvIndex     = srvManager_->AllocateForTexture();
	textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(
		textureData.srvIndex);
	textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(
		textureData.srvIndex);

	// SRV割り当て情報をログ出力
	Console::Print(std::format("LoadTexture: {} にSRVインデックス {} を割り当て\n",
	                           filePath, textureData.srvIndex),
	               kConTextColorCompleted);

	// キューブマップの場合は専用のSRV作成メソッドを呼び出す
	if (isCubeMap) {
		srvManager_->CreateSRVForTextureCube(
			textureData.srvIndex,
			textureData.resource,
			textureData.metadata.format,
			static_cast<UINT>(textureData.metadata.mipLevels)
		);
	} else {
		srvManager_->CreateSRVForTexture2D(
			textureData.srvIndex,
			textureData.resource.Get(),
			textureData.metadata.format,
			static_cast<UINT>(textureData.metadata.mipLevels)
		);
	}

	textureData.resource->SetName(StrUtil::ToWString(filePath).c_str());
}

TexManager::TextureData* TexManager::GetTextureData(
	const std::string& filePath) const {
	// ファイルパスを完全な相対パスに変換
	std::string normalizedPath;
	try {
		// パスを正規化
		std::filesystem::path path(filePath);
		// lexically_normal()で../などのパス要素を解決
		path = path.lexically_normal();

		if (path.is_relative()) {
			// 既に相対パスの場合はそのまま使用
			normalizedPath = path.string();
		} else {
			// 絶対パスの場合は、カレントディレクトリからの相対パスに変換
			normalizedPath = std::filesystem::relative(
				path, std::filesystem::current_path()).string();
		}

		// パスの区切り文字を統一（Windowsの場合も/に統一）
		std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

		// パスが"./"で始まっていない場合は先頭に追加
		if (!normalizedPath.empty() && normalizedPath.substr(0, 2) != "./") {
			normalizedPath = "./" + normalizedPath;
		}
	} catch (const std::exception& e) {
		Console::Print(std::format("GetTextureData: パス変換エラー: {}\n", e.what()));
		assert(0);
		return nullptr;
	}

	// 正規化されたパスでテクスチャを検索
	auto it = textureData_.find(normalizedPath);
	if (it != textureData_.end()) {
		return const_cast<TextureData*>(&it->second);
	}

	// 元のパスでも検索（互換性のため）
	it = textureData_.find(filePath);
	if (it != textureData_.end()) {
		return const_cast<TextureData*>(&it->second);
	}

	Console::Print(std::format("GetTextureData: filePathが見つかりません: {}",
	                           normalizedPath));
	assert(0);
	return nullptr;
}

/// @brief ファイルパスからテクスチャのインデックスを取得します
/// @param filePath テクスチャファイルのパス
/// @return テクスチャのインデックス
uint32_t TexManager::GetTextureIndexByFilePath(
	const std::string& filePath) const {
	// 1. まず完全なパスで検索
	auto it = textureData_.find(filePath);
	if (it != textureData_.end()) {
		return it->second.srvIndex;
	}

	// 2. ファイル名のみで検索（パスの違いを無視）
	std::string filename  = filePath;
	size_t      lastSlash = filePath.find_last_of("/\\");
	if (lastSlash != std::string::npos) {
		filename = filePath.substr(lastSlash + 1);
	}

	for (const auto& [path, data] : textureData_) {
		std::string currentFilename  = path;
		size_t      currentLastSlash = path.find_last_of("/\\");
		if (currentLastSlash != std::string::npos) {
			currentFilename = path.substr(currentLastSlash + 1);
		}

		if (currentFilename == filename) {
			Console::Print(std::format(
				               "GetTextureIndexByFilePath: ファイル名一致で見つかりました: {} -> {} (インデックス {})",
				               filePath, path, data.srvIndex),
			               kConTextColorCompleted);
			return data.srvIndex;
		}
	}

	// パスが正規化されている可能性があるので、部分一致でチェック
	for (const auto& [path, data] : textureData_) {
		if (path.find(filePath) != std::string::npos || filePath.find(path) !=
			std::string::npos) {
			Console::Print(std::format(
				               "GetTextureIndexByFilePath: 部分一致で見つかりました: {} -> {} (インデックス {})",
				               filePath, path, data.srvIndex),
			               kConTextColorWarning);
			return data.srvIndex;
		}
	}

	Console::Print(
		std::format("GetTextureIndexByFilePath: テクスチャが見つかりません: {}", filePath),
		kConTextColorError);
	// デバッグ用に全てのテクスチャのパスを出力
	Console::Print("登録されているテクスチャ一覧:");
	for (const auto& [path, data] : textureData_) {
		Console::Print(std::format("{} (インデックス: {})", path, data.srvIndex));
	}

	// エラーが発生した場合は、テクスチャをロードして再試行
	Console::Print(
		std::format("GetTextureIndexByFilePath: {} を自動ロードして再試行します", filePath),
		kConTextColorWarning);
	const_cast<TexManager*>(this)->LoadTexture(filePath);

	// 再検索
	it = textureData_.find(filePath);
	if (it != textureData_.end()) {
		Console::Print(std::format(
			               "GetTextureIndexByFilePath: 自動ロード後に見つかりました: {} -> インデックス {}",
			               filePath, it->second.srvIndex),
		               kConTextColorCompleted);
		return it->second.srvIndex;
	}

	// 最終手段: smoke.pngとdev_measure.pngの特別処理
	if (filePath.find("dev_measure.png") != std::string::npos) {
		// dev_measure.pngを明示的に検索
		for (const auto& [path, data] : textureData_) {
			if (path.find("dev_measure.png") != std::string::npos) {
				Console::Print(std::format(
					               "GetTextureIndexByFilePath: 特殊処理 - dev_measure.pngを見つけました: {} -> インデックス {}",
					               path, data.srvIndex), kConTextColorWarning);
				return data.srvIndex;
			}
		}
	} else if (filePath.find("smoke.png") != std::string::npos) {
		// smoke.pngを明示的に検索
		for (const auto& [path, data] : textureData_) {
			if (path.find("smoke.png") != std::string::npos) {
				Console::Print(std::format(
					               "GetTextureIndexByFilePath: 特殊処理 - smoke.pngを見つけました: {} -> インデックス {}",
					               path, data.srvIndex), kConTextColorWarning);
				return data.srvIndex;
			}
		}
	}

	// それでも見つからない場合は0を返す
	return 0;
}

/// @brief テクスチャのGPUディスクリプタハンドルを取得します
/// @param filePath
/// @return GPUディスクリプタハンドル
D3D12_GPU_DESCRIPTOR_HANDLE TexManager::GetSrvHandleGPU(
	const std::string& filePath) {
	// ファイル名のみで検索（パスの違いを無視）- GetTextureIndexByFilePathと同様のロジック
	std::string filename  = filePath;
	size_t      lastSlash = filePath.find_last_of("/\\");
	if (lastSlash != std::string::npos) {
		filename = filePath.substr(lastSlash + 1);
	}

	// まず完全一致で検索
	auto it = textureData_.find(filePath);
	if (it != textureData_.end()) {
		return it->second.srvHandleGPU;
	}

	// ファイル名で検索
	for (auto& [path, data] : textureData_) {
		std::string currentFilename  = path;
		size_t      currentLastSlash = path.find_last_of("/\\");
		if (currentLastSlash != std::string::npos) {
			currentFilename = path.substr(currentLastSlash + 1);
		}

		if (currentFilename == filename) {
			Console::Print(std::format(
				               "GetSrvHandleGPU: ファイル名一致で見つかりました: {} -> {}",
				               filePath, path), kConTextColorCompleted);
			return data.srvHandleGPU;
		}
	}

	// パスが正規化されている可能性があるので、部分一致でチェック
	for (auto& [path, data] : textureData_) {
		if (path.find(filePath) != std::string::npos || filePath.find(path) !=
			std::string::npos) {
			Console::Print(std::format(
				               "GetSrvHandleGPU: 部分一致で見つかりました: {} -> {}",
				               filePath, path), kConTextColorWarning);
			return data.srvHandleGPU;
		}
	}

	// 特別なケース: dev_measure.pngとsmoke.png
	if (filePath.find("dev_measure.png") != std::string::npos) {
		for (auto& [path, data] : textureData_) {
			if (path.find("dev_measure.png") != std::string::npos) {
				Console::Print(std::format(
					               "GetSrvHandleGPU: 特殊処理 - dev_measure.pngを見つけました: {} -> {}",
					               filePath, path), kConTextColorWarning);
				return data.srvHandleGPU;
			}
		}
	} else if (filePath.find("smoke.png") != std::string::npos) {
		for (auto& [path, data] : textureData_) {
			if (path.find("smoke.png") != std::string::npos) {
				Console::Print(std::format(
					               "GetSrvHandleGPU: 特殊処理 - smoke.pngを見つけました: {} -> {}",
					               filePath, path), kConTextColorWarning);
				return data.srvHandleGPU;
			}
		}
	}

	Console::Print(
		std::format("GetSrvHandleGPU: filePathが見つかりません: {}", filePath),
		kConTextColorError);
	return {};
}

uint32_t TexManager::GetLoadedTextureCount() const {
	return static_cast<uint32_t>(textureData_.size());
}
