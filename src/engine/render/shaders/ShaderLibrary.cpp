#include "ShaderLibrary.h"

#include <algorithm>
#include <fstream>

#include "core/assets/AssetManager.h"
#include "core/hash/HashBuilder.h"
#include "core/assets/FileStamp.h"
#include "core/hash/StableHashBuilder.h"
#include "core/path/PathUtil.h"
#include "core/string/StrUtil.h"

#include "engine/rhi/DxcShaderCompiler.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Render {
	static constexpr std::string_view kChannel      = "ShaderLibrary";
	static constexpr std::string_view kDxilCacheDir =
		"./content/core/shaders/compiled/";

	ShaderLibrary::ShaderLibrary(
		AssetManager& assetManager, Rhi::DxcShaderCompiler& dxcCompiler
	) : mAssetManager(assetManager),
	    mDxcShaderCompiler(dxcCompiler) {
	}

	const ShaderDxil& ShaderLibrary::GetOrCreateDxil(const ShaderKey& key) {
		auto& reverse = mReverse[key.shaderSourceId];
		if (std::ranges::find(reverse, key) == reverse.end()) {
			reverse.emplace_back(key);
		}

		const auto runtimeIt = mRuntimeCache.find(key);
		const bool hadCached = runtimeIt != mRuntimeCache.end();
		const bool wasDirty  = mDirtyKeys.erase(key) > 0;
		if (hadCached && !wasDirty) {
			return runtimeIt->second;
		}

		const auto dxilPath  = GetDxilCachePath(key);
		ShaderDxil candidate = {};
		bool       prepared  = false;
		if (std::filesystem::exists(dxilPath)) {
			candidate.bytes = ReadFileBytes(dxilPath);
			prepared        = !candidate.bytes.empty();
			if (!prepared) {
				Warning(
					kChannel,
					"DXIL cache file is empty. key(source={}, entry='{}', profile='{}')",
					key.shaderSourceId,
					key.entry,
					key.profile
				);
			}
		}

		if (!prepared) {
			const auto* src = mAssetManager.Get<ShaderSourceAssetData>(
				key.shaderSourceId
			);
			if (!src) {
				Error(
					kChannel, "ShaderSource asset payload missing: id={}",
					key.shaderSourceId
				);
				if (hadCached) {
					return runtimeIt->second;
				}
				auto [it, _] = mRuntimeCache.emplace(key, ShaderDxil{});
				return it->second;
			}

			const std::wstring sourcePath = Path::FromUtf8(src->path).wstring();
			const std::wstring entry      = StrUtil::ToWString(key.entry);
			const std::wstring profile    = StrUtil::ToWString(key.profile);

			const std::vector<std::wstring> includeDirs;
			const auto                      extraArgs = BuildDxcArgs(key);

			if (!mDxcShaderCompiler.Initialize()) {
				Error(kChannel, "DxcShaderCompiler initialization failed.");
			} else {
				const bool ok = mDxcShaderCompiler.CompileToFileDXIL(
					sourcePath, entry, profile, includeDirs, extraArgs,
					dxilPath.wstring()
				);
				if (ok) {
					candidate.bytes = ReadFileBytes(dxilPath);
					prepared        = !candidate.bytes.empty();
					if (!prepared) {
						Error(
							kChannel,
							"Compiled DXIL is empty. key(source={}, entry='{}', profile='{}')",
							key.shaderSourceId,
							key.entry,
							key.profile
						);
					}
				}
			}
		}

		if (prepared) {
			if (hadCached) {
				runtimeIt->second = std::move(candidate);
				return runtimeIt->second;
			}
			auto [it, _] = mRuntimeCache.emplace(key, std::move(candidate));
			return it->second;
		}

		if (hadCached) {
			Warning(
				kChannel,
				"Shader compile failed. Keeping last known good DXIL. key(source={}, entry='{}', profile='{}')",
				key.shaderSourceId,
				key.entry,
				key.profile
			);
			return runtimeIt->second;
		}

		Warning(
			kChannel,
			"Shader compile failed and no fallback exists. key(source={}, entry='{}', profile='{}')",
			key.shaderSourceId,
			key.entry,
			key.profile
		);
		auto [it, _] = mRuntimeCache.emplace(key, ShaderDxil{});
		return it->second;
	}

	void ShaderLibrary::MarkDirtyByShaderSource(const AssetID shaderSourceId) {
		const auto it = mReverse.find(shaderSourceId);
		if (it == mReverse.end()) {
			return;
		}

		for (const auto& key : it->second) {
			mDirtyKeys.emplace(key);
		}
	}

	void ShaderLibrary::MarkAllDirty() {
		for (const auto& [key, _] : mRuntimeCache) {
			mDirtyKeys.emplace(key);
		}
	}

	void ShaderLibrary::InvalidateByShaderSource(const AssetID shaderSourceId) {
		MarkDirtyByShaderSource(shaderSourceId);
	}

	void ShaderLibrary::InvalidateAll() {
		mRuntimeCache.clear();
		mDirtyKeys.clear();
		mReverse.clear();
	}

	void ShaderLibrary::SetCacheDirectory(std::filesystem::path dir) {
		mCacheDir = std::move(dir);
	}

	std::filesystem::path ShaderLibrary::GetDxilCachePath(
		const ShaderKey& key
	) const {
		const uint64_t dh = ComputeDerivedHash(key);
		return mCacheDir / StrUtil::ToWString(
			       std::string(kDxilCacheDir) +
			       "shader_" +
			       std::to_string(key.shaderSourceId) +
			       "_" +
			       key.entry +
			       "_" +
			       key.profile +
			       "_" +
			       std::to_string(dh) +
			       ".dxil"
		       );
	}

	uint64_t ShaderLibrary::ComputeDerivedHash(const ShaderKey& key) const {
		StableHashBuilder hashBuilder;

		const auto* src = mAssetManager.Get<ShaderSourceAssetData>(
			key.shaderSourceId
		);
		if (!src) {
			Error(
				kChannel, "ShaderSource payload missing while hashing: id={}",
				key.shaderSourceId
			);
			return 0;
		}

		// ソースファイルのパスを正規化してハッシュに含める
		const std::string srcPath = StrUtil::NormalizePath(src->path);
		hashBuilder.AddString(srcPath);
		hashBuilder.AddString(key.entry);
		hashBuilder.AddString(key.profile);
		for (const auto& [name, value] : key.defines) {
			hashBuilder.AddString(name);
			hashBuilder.AddString(value);
		}

		const auto& meta = mAssetManager.Meta(key.shaderSourceId);

		hashBuilder.AddUInt64(meta.fileStamp.sizeInBytes);
		hashBuilder.AddInt64(meta.fileStamp.lastWriteTicks);

		for (
			const auto depId : mAssetManager.GetDependencies(key.shaderSourceId)
		) {
			const auto& depMeta = mAssetManager.Meta(depId);
			hashBuilder.AddUInt64(depId);
			hashBuilder.AddUInt64(depMeta.fileStamp.sizeInBytes);
			hashBuilder.AddInt64(depMeta.fileStamp.lastWriteTicks);
		}

		return hashBuilder.Value();
	}

	std::vector<std::wstring> ShaderLibrary::BuildDxcArgs(
		const ShaderKey& key
	) {
		std::vector<std::wstring> args;
		constexpr int             baseArgReserve      = 16; // 予備容量
		constexpr int             entryProfileArgCost = 2;  // エントリポイントとプロファイル
		args.reserve(baseArgReserve + key.defines.size() * entryProfileArgCost);

		// defines
		for (const auto& [name, value] : key.defines) {
			std::wstring d = L"-D";
			std::wstring nv;
			nv.assign(name.begin(), name.end());
			nv += L"=";
			nv.append(value.begin(), value.end());
			args.emplace_back(d);
			args.emplace_back(nv);
		}

		args.emplace_back(L"-WX"); // 警告をエラーとして扱う
		args.emplace_back(L"-Zi"); // デバッグ情報を生成

#ifdef _DEBUG
		args.emplace_back(L"-Od"); // 最適化を外す
#else
		args.emplace_back(L"-O3"); // 最適化有効
#endif

		args.emplace_back(L"-Zpr"); // メモリレイアウトは行優先

		return args;
	}

	std::vector<uint8_t> ShaderLibrary::ReadFileBytes(
		const std::filesystem::path& path
	) {
		std::ifstream ifs(path, std::ios::binary);
		if (!ifs) {
			return {};
		}
		ifs.seekg(0, std::ios::end);
		const auto size = static_cast<size_t>(ifs.tellg());
		ifs.seekg(0, std::ios::beg);

		std::vector<uint8_t> bytes(size);
		if (size > 0) {
			ifs.read(
				reinterpret_cast<char*>(bytes.data()),
				static_cast<std::streamsize>(size)
			);
		}
		return bytes;
	}

	void ShaderLibrary::WriteFileBytes(
		const std::filesystem::path& path, const std::vector<uint8_t>& bytes
	) {
		std::filesystem::create_directories(path.parent_path());
		std::ofstream ofs(path, std::ios::binary);
		if (!ofs) {
			return;
		}
		ofs.write(
			reinterpret_cast<const char*>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
	}
}
