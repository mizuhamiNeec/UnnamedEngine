#include <pch.h>

//-----------------------------------------------------------------------------

#include <d3dx12.h>
#include <filesystem>
#include <format>

#include <engine/public/renderer/D3D12.h>
#include <engine/public/renderer/SrvManager.h>
#include <engine/public/TextureManager/TexManager.h>
#include <engine/public/utils/Properties.h>

TexManager* TexManager::mInstance = nullptr;

/// @brief シングルトンインスタンスを取得します
/// @return TexManagerのインスタンス
TexManager* TexManager::GetInstance() {
	if (mInstance == nullptr) {
		mInstance = new TexManager;
	}
	return mInstance;
}

/// @brief テクスチャマネージャーを初期化します
/// @param renderer D3D12レンダラーのポインタ
/// @param srvManager Srvマネージャのポインタ
void TexManager::Init(D3D12* renderer, SrvManager* srvManager) {
	mRenderer   = renderer;
	mSrvManager = srvManager;

	// SRVの数と同数
	//-------------------------------------------------------------------------
	// 最大枚数分のメモリ領域を確保しておく。
	// std::vectorの欠点としてあらかじめ確保していた要素数を超えたときにメモリの再割り当てが発生することが挙げられる。
	// 最初に最大個数のメモリを確保しておくことでその欠点を塞いでおく
	//-------------------------------------------------------------------------
	mTextureData.reserve(kMaxSrvCount);
}

/// @brief 指定されたテクスチャのメタデータを取得します
/// @param filePath
/// @return テクスチャのメタデータ
const DirectX::TexMetadata& TexManager::GetMetaData(
	const std::string& filePath) const {
	auto it = mTextureData.find(filePath);
	assert(it != mTextureData.end()); // ファイルが存在することを確認
	const TextureData& textureData = it->second;
	return textureData.metadata;
}

/// @brief テクスチャリソースを作成します
/// @param metadata テクスチャのメタデータ
/// @return 作成されたテクスチャリソース
Microsoft::WRL::ComPtr<ID3D12Resource> TexManager::CreateTextureResource(
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
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = mRenderer->GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource)
	);

	if (FAILED(hr)) {
		Fatal(
			GetName(),
			"Failed to create texture resource: {}",
			std::format("HRESULT: 0x{:08X}", hr)
		);
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
	const uint32_t        index
) {
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
	const uint32_t        index
) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->
		GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += static_cast<unsigned long long>(descriptorSize) * index;
	return handleGPU;
}

/// @brief テクスチャマネージャーを終了します
void TexManager::Shutdown() {
	delete mInstance;
	mInstance = nullptr;
}

/// @brief テクスチャデータをアップロードします
/// @param texture テクスチャリソース
/// @param mipImages MIPマップイメージ
/// @return 中間リソース
Microsoft::WRL::ComPtr<ID3D12Resource> TexManager::UploadTextureData(
	const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
	const DirectX::ScratchImage&                  mipImages
) const {
	// 中間リソースの作成
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = nullptr;

	std::vector<D3D12_SUBRESOURCE_DATA> subResources;
	DirectX::PrepareUpload(mRenderer->GetDevice(), mipImages.GetImages(),
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
	HRESULT hr = mRenderer->GetDevice()->CreateCommittedResource(
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
	UpdateSubresources(mRenderer->GetCommandList(),
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
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	mRenderer->GetCommandList()->ResourceBarrier(1, &barrier);

	return intermediateResource;
}

/// @brief テクスチャを読み込みます
/// @param filePath テクスチャファイルのパス
/// @param forceCubeMap キューブマップとして強制的に読み込むかどうか
void TexManager::LoadTexture(const std::string& filePath, bool forceCubeMap) {
	mRenderer->WaitPreviousFrame();

	// 読み込み済みテクスチャを検索
	if (mTextureData.contains(filePath)) {
		return; // 読み込み済みなら早期リターン
	}

	// テクスチャ枚数上限チェック
	assert(mSrvManager->CanAllocate());

	// ファイル拡張子を取得して小文字に変換
	std::string extension = filePath.substr(
		filePath.find_last_of('.') + 1);
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
		Msg(
			GetName(),
			"Failed to load texture: {} (HRESULT: 0x{:08X})",
			filePath, hr
		);
		// デフォルトテクスチャの読み込み
		filePathW = StrUtil::ToWString("./resources/textures/error.png");
		hr        = DirectX::LoadFromWICFile(
			filePathW.c_str(),
			DirectX::WIC_FLAGS_FORCE_SRGB,
			nullptr,
			image
		);
		assert(SUCCEEDED(hr)); // デフォルトのテクスチャも読み込めなかった場合はエラー
		isCubeMap = false;     // デフォルトテクスチャはキューブマップではない
	}

	DirectX::ScratchImage mipImages = {};

	// DDSファイルで既にミップマップがある場合は生成しない
	if (extension == "dds" && image.GetMetadata().mipLevels > 1) {
		// 既存のミップマップを使用
		mipImages = std::move(image);
	} else {
		// 小さい画像の場合はミップマップを生成しない
		const DirectX::TexMetadata& metadata = image.GetMetadata();

		if (metadata.width >= kMinTextureSizeForMipmap && metadata.height >=
			kMinTextureSizeForMipmap) {
			hr = DirectX::GenerateMipMaps(image.GetImages(),
			                              image.GetImageCount(),
			                              metadata,
			                              DirectX::TEX_FILTER_SRGB, 0,
			                              mipImages);
			assert(SUCCEEDED(hr));
		} else {
			DevMsg(
				GetName(),
				"画像が小さいためMipMapを生成しません: {} ({}x{})",
				filePath, metadata.width, metadata.height
			);
			mipImages = std::move(image);
		}
	}

	// テクスチャデータを追加
	TextureData& textureData = mTextureData[filePath];

	textureData.filePath = filePath;
	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = CreateTextureResource(textureData.metadata);

	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource =
		UploadTextureData(
			textureData.resource, mipImages);

	// コマンドリストを閉じる
	hr = mRenderer->GetCommandList()->Close();
	assert(SUCCEEDED(hr));
	if (hr) {
		Fatal(
			GetName(),
			"Failed to close command list for texture upload: {}",
			std::format("HRESULT: 0x{:08X}", hr)
		);
	}

	// コマンドのキック
	ID3D12CommandList* lists[] = {mRenderer->GetCommandList()};
	mRenderer->GetCommandQueue()->ExecuteCommandLists(1, lists);

	// 実行を待つ
	mRenderer->WaitPreviousFrame();

	// コマンドのリセット
	hr = mRenderer->GetCommandList()->Reset(
		mRenderer->GetCommandAllocator(),
		nullptr
	);
	assert(SUCCEEDED(hr));

	// リソース解放
	intermediateResource.Reset();

	// SRV確保（テクスチャタイプに応じて適切なAllocateを使用）
	if (isCubeMap) {
		textureData.srvIndex = mSrvManager->AllocateForTextureCube();
	} else {
		textureData.srvIndex = mSrvManager->AllocateForTexture2D();
	}

	textureData.srvHandleCPU = mSrvManager->GetCPUDescriptorHandle(
		textureData.srvIndex);
	textureData.srvHandleGPU = mSrvManager->GetGPUDescriptorHandle(
		textureData.srvIndex);

	// キューブマップの場合は専用のSRV作成メソッドを呼び出す
	if (isCubeMap) {
		mSrvManager->CreateSRVForTextureCube(
			textureData.srvIndex,
			textureData.resource,
			textureData.metadata.format,
			static_cast<UINT>(textureData.metadata.mipLevels)
		);
	} else {
		mSrvManager->CreateSRVForTexture2D(
			textureData.srvIndex,
			textureData.resource.Get(),
			textureData.metadata.format,
			static_cast<UINT>(textureData.metadata.mipLevels)
		);
	}

	textureData.resource->SetName(StrUtil::ToWString(filePath).c_str());
}

TexManager::TextureData* TexManager::GetTextureData(
	const std::string& filePath) {
	// ファイルパスを完全な相対パスに変換
	std::string normalizedPath = filePath;
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
		std::ranges::replace(normalizedPath, '\\', '/');

		// パスが"./"で始まっていない場合は先頭に追加
		if (!normalizedPath.empty() && normalizedPath.substr(0, 2) != "./") {
			normalizedPath = "./" + normalizedPath;
		}
	} catch (const std::exception& e) {
		Error(
			GetName(),
			"GetTextureData: パス変換エラー: ファイルパス: {}, エラー: {}",
			filePath, e.what()
		);
		assert(0);
		return nullptr;
	}

	// 正規化されたパスでテクスチャを検索
	auto it = mTextureData.find(normalizedPath);
	if (it != mTextureData.end()) {
		return &it->second;
	}

	// 元のパスでも検索
	it = mTextureData.find(filePath);
	if (it != mTextureData.end()) {
		return &it->second;
	}

	Error(
		GetName(),
		"GetTextureData: filePathが見つかりません: {}",
		normalizedPath
	);

	assert(0);
	return nullptr;
}

/// @brief ファイルパスからテクスチャのインデックスを取得します
/// @param filePath テクスチャファイルのパス
/// @return テクスチャのインデックス
uint32_t TexManager::GetTextureIndexByFilePath(
	const std::string& filePath) const {
	// 1. まず完全なパスで検索
	auto it = mTextureData.find(filePath);
	if (it != mTextureData.end()) {
		return it->second.srvIndex;
	}

	// 2. ファイル名のみで検索（パスの違いを無視）
	std::string filename  = filePath;
	size_t      lastSlash = filePath.find_last_of("/\\");
	if (lastSlash != std::string::npos) {
		filename = filePath.substr(lastSlash + 1);
	}

	for (const auto& [path, data] : mTextureData) {
		std::string currentFilename  = path;
		size_t      currentLastSlash = path.find_last_of("/\\");
		if (currentLastSlash != std::string::npos) {
			currentFilename = path.substr(currentLastSlash + 1);
		}

		if (currentFilename == filename) {
			DevMsg(
				GetName(),
				"GetTextureIndexByFilePath: ファイル名一致で見つかりました: {} -> {} (インデックス {})",
				filePath, path, data.srvIndex
			);
			return data.srvIndex;
		}
	}

	// パスが正規化されている可能性があるので、部分一致でチェック
	for (const auto& [path, data] : mTextureData) {
		if (path.find(filePath) != std::string::npos || filePath.find(path) !=
			std::string::npos) {
			DevMsg(
				GetName(),
				"GetTextureIndexByFilePath: 部分一致で見つかりました: {} -> {} (インデックス {})",
				filePath, path, data.srvIndex
			);
			return data.srvIndex;
		}
	}

	Error(
		GetName(),
		"GetTextureIndexByFilePath: テクスチャが見つかりません: {}",
		filePath
	);

	// デバッグ用に全てのテクスチャのパスを出力
	DevMsg(GetName(), "登録されているテクスチャ一覧:");
	for (const auto& [path, data] : mTextureData) {
		DevMsg(
			GetName(),
			"  - {} (インデックス: {})",
			path, data.srvIndex
		);
	}

	// エラーが発生した場合は、テクスチャをロードして再試行
	Warning(
		GetName(),
		"GetTextureIndexByFilePath: テクスチャが見つからないため、{} を自動ロードします",
		filePath
	);

	const_cast<TexManager*>(this)->LoadTexture(filePath);

	// 再検索
	it = mTextureData.find(filePath);
	if (it != mTextureData.end()) {
		return it->second.srvIndex;
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
	auto it = mTextureData.find(filePath);
	if (it != mTextureData.end()) {
		return it->second.srvHandleGPU;
	}

	// ファイル名で検索
	for (auto& [path, data] : mTextureData) {
		std::string currentFilename  = path;
		size_t      currentLastSlash = path.find_last_of("/\\");
		if (currentLastSlash != std::string::npos) {
			currentFilename = path.substr(currentLastSlash + 1);
		}

		if (currentFilename == filename) {
			DevMsg(
				GetName(),
				"GetSrvHandleGPU: ファイル名一致で見つかりました: {} -> {}",
				filePath, path
			);
			return data.srvHandleGPU;
		}
	}

	// パスが正規化されている可能性があるので、部分一致でチェック
	for (auto& [path, data] : mTextureData) {
		if (path.find(filePath) != std::string::npos || filePath.find(path) !=
			std::string::npos) {
			DevMsg(
				GetName(),
				"GetSrvHandleGPU: 部分一致で見つかりました: {} -> {}",
				filePath, path
			);

			return data.srvHandleGPU;
		}
	}

	Error(
		GetName(),
		"GetSrvHandleGPU: テクスチャが見つかりません: {}",
		filePath
	);
	return {};
}

uint32_t TexManager::GetLoadedTextureCount() const {
	return static_cast<uint32_t>(mTextureData.size());
}

Microsoft::WRL::ComPtr<ID3D12Resource> TexManager::GetTextureResource(
	const std::string& filePath) const {
	// まず完全なパスで検索
	auto it = mTextureData.find(filePath);
	if (it != mTextureData.end()) {
		return it->second.resource;
	}

	// ファイル名のみで検索（パスの違いを無視）
	std::string filename  = filePath;
	size_t      lastSlash = filePath.find_last_of("/\\");
	if (lastSlash != std::string::npos) {
		filename = filePath.substr(lastSlash + 1);
	}

	// ファイル名で検索
	for (const auto& [path, data] : mTextureData) {
		std::string currentFilename  = path;
		size_t      currentLastSlash = path.find_last_of("/\\");
		if (currentLastSlash != std::string::npos) {
			currentFilename = path.substr(currentLastSlash + 1);
		}

		if (currentFilename == filename) {
			return data.resource;
		}
	}

	Error(
		GetName(),
		"GetTextureResource: テクスチャが見つかりません: {}",
		filePath
	);
	return nullptr; // 見つからない場合はnullptrを返す
}

uint32_t TexManager::GetTextureSrvIndex(const std::string& filePath) const {
	return GetTextureIndexByFilePath(filePath);
}

void TexManager::UpdateTextureSrvIndex(
	const std::string& filePath,
	const uint32_t     newSrvIndex
) {
	auto it = mTextureData.find(filePath);
	if (it != mTextureData.end()) {
		it->second.srvIndex = newSrvIndex;
	} else {
		Error(
			GetName(),
			"UpdateTextureSrvIndex: テクスチャが見つかりません: {}",
			filePath
		);
	}
}
