#include <pch.h>

//-----------------------------------------------------------------------------

#include "ShaderLibrary.h"

#include <d3d12.h>
#include <ranges>

#include <engine/public/subsystem/console/Log.h>
#include <engine/public/uengine/UEngine.h>

#include "runtime/assets/core/UAssetManager.h"

namespace Unnamed {
	constexpr std::string_view kChannel = "ShaderLibrary";

	using namespace Microsoft::WRL;

	ComPtr<IDxcBlob> CompileHLSL(
		const std::string&      src,
		const ShaderVariantKey& key,
		const char*             virtualName = nullptr
	) {
		std::vector<D3D_SHADER_MACRO> macros;
		macros.reserve(key.defines.size() + 1);
		for (auto& define : key.defines) {
			const auto pos = define.find('=');
			if (pos == std::string::npos) {
				macros.emplace_back(define.c_str(), "1");
			} else {
				std::string name  = define.substr(0, pos);
				std::string value = define.substr(pos + 1);
				// TODO: ここで name/val の寿命を維持するなら別保管が必要。
				// 最小実装では defines は呼び出しスコープのライフタイムに依存。
				macros.emplace_back(name.c_str(), value.c_str());
			}
		}
		macros.emplace_back(nullptr, nullptr);

		ComPtr<IDxcUtils>          dxcUtils;
		ComPtr<IDxcCompiler3>      dxcCompiler;
		ComPtr<IDxcIncludeHandler> includeHandler;
		THROW(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils)));
		THROW(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler)));
		THROW(dxcUtils->CreateDefaultIncludeHandler(&includeHandler));

		ComPtr<IDxcBlobEncoding> sourceBlob;
		THROW(
			dxcUtils->CreateBlobFromPinned(
				src.c_str(), static_cast<UINT32>(src.size()),
				DXC_CP_UTF8, &sourceBlob
			)
		);

		DxcBuffer shaderSourceBuffer = {};
		shaderSourceBuffer.Ptr       = sourceBlob->GetBufferPointer();
		shaderSourceBuffer.Size      = sourceBlob->GetBufferSize();
		shaderSourceBuffer.Encoding  = DXC_CP_UTF8;

		const std::wstring wEntryPoint = StrUtil::ToWString(key.entryPoint);
		const std::wstring wTarget     = StrUtil::ToWString(key.target);

		bool         hasName = virtualName != nullptr;
		std::wstring wVirtualName;
		if (hasName) {
			wVirtualName = StrUtil::ToWString(virtualName);
		}

		std::array arguments = {
			hasName ? wVirtualName.c_str() : L"Generated.hlsl", // 仮想ファイル名
			L"-E", wEntryPoint.c_str(), // エントリーポイントの指定。
			L"-T", wTarget.c_str(), // ShaderProfileの設定
			DXC_ARG_DEBUG, L"-Qembed_debug", // デバッグ用の情報を埋め込む
			DXC_ARG_SKIP_OPTIMIZATIONS, // 最適化を外しておく
			DXC_ARG_PACK_MATRIX_ROW_MAJOR, // 行列のメモリレイアウトは行優先
		};

		ComPtr<IDxcResult> shaderResult;
		THROW(
			dxcCompiler->Compile(
				&shaderSourceBuffer,
				arguments.data(),                      // 引数
				static_cast<UINT32>(arguments.size()), // 引数の数
				includeHandler.Get(),                  // includeが含まれた諸々
				IID_PPV_ARGS(&shaderResult)            // コンパイル結果
			)
		);

		// コンパイル時の
		ComPtr<IDxcBlobUtf8> shaderError;
		shaderResult->GetOutput(
			DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr
		);
		if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
			Fatal(kChannel, "{}", shaderError->GetStringPointer());
			return nullptr;
		}

		ComPtr<IDxcBlob> shaderBlob;
		THROW(
			shaderResult->GetOutput(
				DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr
			)
		);
		Msg(
			kChannel,
			"Shader compile success: entryPoint='{}', target='{}'",
			key.entryPoint.c_str(), key.target.c_str()
		);

		return shaderBlob;
	}

	void HashCombine(size_t& h, const size_t v) {
		h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
	}

	ShaderLibrary::ShaderLibrary(
		GraphicsDevice* graphicsDevice, UAssetManager* assetManager
	) : mGraphicsDevice(graphicsDevice), mAssetManager(assetManager) {
		UASSERT(graphicsDevice);
		UASSERT(assetManager);
	}

	const ShaderBlob* ShaderLibrary::GetOrCompile(
		const ShaderVariantKey& key
	) {
		if (key.asset == kInvalidAssetID) {
			return nullptr;
		}

		const size_t hash = Hash(key);
		auto         it   = mCache.find(hash);
		if (it != mCache.end() && it->second.shaderBlob.blob) {
			return &it->second.shaderBlob;
		}

		const auto* s = mAssetManager->Get<ShaderAssetData>(key.asset);
		if (!s) {
			return nullptr;
		}

		auto blob = CompileHLSL(s->hlsl, key);
		if (!blob) {
			return nullptr;
		}

		Entry entry;
		entry.hash            = hash;
		entry.shaderBlob.blob = std::move(blob);
		mCache[hash]          = std::move(entry);
		return &mCache[hash].shaderBlob;
	}

	const ShaderBlob* ShaderLibrary::GetOrCompileFromString(
		const std::string&      hlsl,
		const ShaderVariantKey& key,
		const char*             virtualName
	) {
		// key でハッシュを作る
		size_t h = Hash(key);
		// 文字列を足す
		size_t strH = std::hash<std::string>{}(hlsl);
		HashCombine(h, strH);
		if (virtualName) {
			size_t nameH = std::hash<std::string>{}(virtualName);
			HashCombine(h, nameH);
		}

		// キャッシュ検索
		auto it = mCache.find(h);
		if (it != mCache.end() && it->second.shaderBlob.blob) {
			return &it->second.shaderBlob;
		}

		// コンパイル
		auto blob = CompileHLSL(hlsl, key);
		if (!blob) {
			return nullptr;
		}

		Entry entry;
		entry.hash            = h;
		entry.shaderBlob.blob = std::move(blob);
		mCache[h]             = std::move(entry);
		return &mCache[h].shaderBlob;
	}

	// TODO: 使わないなら消そう
	void ShaderLibrary::InvalidateByAsset([[maybe_unused]] AssetID asset) {
		std::vector<size_t> toRemove;
		toRemove.reserve(mCache.size());
		for (const auto& h : mCache | std::views::keys) {
			// e から asset を辿れないので、現設計では完全消去が簡単。
			// より厳密にするなら hash に asset も埋めていたので、hash 再構成でもok.
			toRemove.emplace_back(h);
		}
		for (auto h : toRemove) {
			mCache.erase(h);
		}
	}

	size_t ShaderLibrary::Hash(const ShaderVariantKey& key) {
		constexpr std::hash<std::string> hashString;
		constexpr std::hash<uint64_t>    hashUint64;
		size_t                           h = hashUint64(key.asset);
		HashCombine(h, hashString(key.entryPoint));
		HashCombine(h, hashString(key.target));
		for (auto& define : key.defines) {
			HashCombine(h, hashString(define));
		}
		return h;
	}
}
