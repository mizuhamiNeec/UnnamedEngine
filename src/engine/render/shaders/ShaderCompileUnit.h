#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/assets/AssetID.h"
#include "core/filesystem/Path.h"
#include "core/filesystem/VirtualPath.h"

namespace Unnamed {
	class AssetManager;
}

namespace Unnamed::Render {
	/// @brief DXCへ渡す事前解決済みinclude sourceです。
	struct ShaderCompileIncludeEntry final {
		std::string                internalName;
		std::optional<VirtualPath> virtualPath;
		Path                       physicalPath;
		AssetID                    assetId = kInvalidAssetID;
		std::string                rewrittenSource;
	};

	/// @brief 1回のShader compileで使用するroot sourceとinclude tableです。
	struct ShaderCompileUnit final {
		std::string rewrittenRootSource;
		std::string rootDiagnosticPath;
		std::string rootMountId;
		std::vector<ShaderCompileIncludeEntry> includeEntries;
		std::unordered_map<std::string, size_t> includeIndexByInternalName;
	};

	/// @brief AssetManagerの確定済み依存グラフからShader compile unitを構築します。
	class ShaderCompileUnitBuilder final {
	public:
		/// @brief コンストラクタ。
		/// @param assetManager ShaderSourceと依存グラフの取得元。
		explicit ShaderCompileUnitBuilder(const AssetManager& assetManager);

		/// @brief root ShaderSource用compile unitを構築します。
		/// @param rootShaderSourceId root ShaderSource AssetID。
		/// @return 構築済みcompile unit。依存不整合時はstd::nullopt。
		[[nodiscard]] std::optional<ShaderCompileUnit> Build(
			AssetID rootShaderSourceId
		) const;

	private:
		/// @brief include tokenを内部名へ置換し診断用#lineを付加します。
		[[nodiscard]] std::optional<std::string> RewriteSource(
			AssetID sourceAssetId,
			const std::unordered_map<AssetID, std::string>& internalNames
		) const;

		/// @brief #lineへ埋め込める診断パスを生成します。
		[[nodiscard]] std::string BuildDiagnosticPath(
			AssetID sourceAssetId
		) const;

		const AssetManager& mAssetManager;
	};
}
