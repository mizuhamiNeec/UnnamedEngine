#pragma once
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/filesystem/Path.h"
#include "ShaderKey.h"

namespace Unnamed {
	namespace Rhi {
		class DxcShaderCompiler;
	}

	class AssetManager;
}

namespace Unnamed::Render {
	/// @brief ShaderDxilは、compile済みDXIL bytecodeとshader cache識別情報を所有します
	struct ShaderDxil {
		std::vector<uint8_t> bytes;
	};

	/// @brief ShaderLibraryは、コンパイル済みDXILと依存ファイル指紋をキー別にキャッシュします
	class ShaderLibrary {
	public:
		ShaderLibrary(
			AssetManager& assetManager, Rhi::DxcShaderCompiler& dxcCompiler
		);

		const ShaderDxil& GetOrCreateDxil(const ShaderKey& key);

		void MarkDirtyByShaderSource(AssetID shaderSourceId);
		void MarkAllDirty();

		void InvalidateByShaderSource(AssetID shaderSourceId);

		void InvalidateAll();
		void SetCacheDirectory(Path dir);

	private:
		/// @brief ShaderDependencyFingerprintは、shader依存fileのpath、更新時刻、content hashを保持します
		struct ShaderDependencyFingerprint final {
			std::string mountId;
			std::string stablePath;
			AssetID     assetId = kInvalidAssetID;
			uint32_t    version = 0;
			uint64_t    sizeInBytes = 0;
			int64_t     lastWriteTicks = 0;
		};

		[[nodiscard]] Path     GetDxilCachePath(const ShaderKey& key) const;
		[[nodiscard]] uint64_t ComputeDerivedHash(const ShaderKey& key) const;
		/// @brief Root ShaderSourceを含む推移的依存fingerprintを安定順で収集します。
		[[nodiscard]] std::vector<ShaderDependencyFingerprint>
		CollectDependencyFingerprints(AssetID rootShaderSourceId) const;

		static std::vector<std::wstring> BuildDxcArgs(const ShaderKey& key);

		static std::vector<uint8_t> ReadFileBytes(const Path& path);
		static void                 WriteFileBytes(
			const Path& path, const std::vector<uint8_t>& bytes
		);

		AssetManager&           mAssetManager;
		Rhi::DxcShaderCompiler& mDxcShaderCompiler;

		Path mCacheDir = {};

		std::unordered_map<ShaderKey, ShaderDxil, ShaderKeyHash> mRuntimeCache;
		std::unordered_set<ShaderKey, ShaderKeyHash>             mDirtyKeys;

		std::unordered_map<AssetID, std::vector<ShaderKey>> mReverse;
	};
}
