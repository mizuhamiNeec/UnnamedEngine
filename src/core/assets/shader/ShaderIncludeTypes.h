#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "core/assets/AssetID.h"
#include "core/filesystem/Path.h"
#include "core/filesystem/VirtualPath.h"

namespace Unnamed {
	/// @brief HLSL include参照の基準を表します。
	enum class ShaderIncludeKind : uint8_t {
		SourceRelative,
		MountRootRelative,
	};

	/// @brief HLSLソースから抽出したinclude参照です。
	struct ShaderIncludeReference final {
		ShaderIncludeKind kind = ShaderIncludeKind::SourceRelative;
		std::string       path;
		std::size_t       sourceTokenBegin = 0;
		std::size_t       sourceTokenEnd   = 0;
	};

	/// @brief mount内で解決されたShader includeです。
	struct ResolvedShaderInclude final {
		ShaderIncludeReference    reference;
		std::optional<VirtualPath> virtualPath;
		Path                       physicalPath;
		AssetID shaderSourceAssetId = kInvalidAssetID;
		std::string mountId;
	};

	/// @brief 解決できなかったShader includeの再試行情報です。
	struct UnresolvedShaderInclude final {
		ShaderIncludeKind kind = ShaderIncludeKind::SourceRelative;
		std::string       path;
		std::string       mountId;
		Path              expectedPhysicalPath;
		Path              watchedParentDirectory;
	};
}
