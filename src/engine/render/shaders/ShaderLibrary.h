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
	struct ShaderDxil {
		std::vector<uint8_t> bytes;
	};

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
		[[nodiscard]] Path     GetDxilCachePath(const ShaderKey& key) const;
		[[nodiscard]] uint64_t ComputeDerivedHash(const ShaderKey& key) const;

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
