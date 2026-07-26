#pragma once
#include <string>
#include <vector>

#include "core/assets/AssetID.h"

namespace Unnamed::Render {
	/// @brief ShaderKeyは、shader compile結果を一意に決めるpath、entry point、profile、define列を保持します
	struct ShaderKey {
		AssetID shaderSourceId = kInvalidAssetID;
		std::string entry; // エントリポイント名
		std::string profile; // シェーダープロファイル
		std::vector<std::pair<std::string, std::string>> defines; // プリプロセッサ定義

		bool operator==(const ShaderKey& rhs) const {
			return shaderSourceId ==
			       rhs.shaderSourceId &&
			       entry == rhs.entry &&
			       profile == rhs.profile &&
			       defines == rhs.defines;
		}
	};

	/// @brief ShaderKeyHashは、シェーダーパス、エントリーポイント、プロファイル、define列からShaderKeyのハッシュを計算します
	struct ShaderKeyHash {
		size_t operator()(const ShaderKey& key) const noexcept;
	};
}
