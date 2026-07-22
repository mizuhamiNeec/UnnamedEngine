#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/assets/shader/ShaderIncludeTypes.h"
#include "core/filesystem/Path.h"
#include "core/filesystem/VirtualPath.h"

namespace Unnamed {
	/// @brief シェーダーソースアセットのデータ構造体
	struct ShaderSourceAssetData {
		Path                       path;
		std::optional<VirtualPath> virtualPath;
		std::string                mountId;
		std::string                sourceText;
		std::vector<ShaderIncludeReference> includeReferences;
		std::vector<ResolvedShaderInclude>  resolvedIncludes;
		std::vector<UnresolvedShaderInclude> unresolvedIncludes;
	};
}
