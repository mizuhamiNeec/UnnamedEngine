#pragma once

#include <dxcapi.h>

#include <runtime/assets/core/UAssetID.h>

#include <wrl/client.h>

namespace Unnamed {
	class UAssetManager;
	class GraphicsDevice;

	/// @brief シェーダーバリアントのキー情報
	struct ShaderVariantKey {
		AssetID                  asset;      // シェーダーアセット
		std::vector<std::string> defines;    // コンパイル定義
		std::string              entryPoint; // エントリポイント
		std::string              target;     // SMのバージョンなど
	};

	/// @brief コンパイル済みシェーダーブロブ情報
	struct ShaderBlob {
		Microsoft::WRL::ComPtr<IDxcBlob> blob;
		uint32_t                         version = 0; // リロード時に増えます
	};

	/// @brief シェーダーライブラリクラス
	class ShaderLibrary {
	public:
		ShaderLibrary(
			GraphicsDevice* graphicsDevice,
			UAssetManager*  assetManager
		);
		const ShaderBlob* GetOrCompile(const ShaderVariantKey& key);
		const ShaderBlob* GetOrCompileFromString(
			const std::string&      hlsl,
			const ShaderVariantKey& key,
			const char*             virtualName = nullptr
		);

	private:
		static size_t Hash(const ShaderVariantKey& key);

		GraphicsDevice* mGraphicsDevice = nullptr;
		UAssetManager*  mAssetManager   = nullptr;

		struct Entry {
			size_t     hash;
			ShaderBlob shaderBlob;
		};

		std::unordered_map<size_t, Entry> mCache;
	};
}
