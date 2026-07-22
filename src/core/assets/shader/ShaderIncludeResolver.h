#pragma once

#include <optional>

#include "ShaderIncludeTypes.h"

namespace Unnamed {
	class ContentPathResolver;

	/// @brief Shader includeを親ShaderSourceと同じmount内で解決します。
	class ShaderIncludeResolver final {
	public:
		/// @brief コンストラクタ。
		/// @param contentPathResolver mount解決サービス。
		explicit ShaderIncludeResolver(
			const ContentPathResolver& contentPathResolver
		);

		/// @brief include参照を物理パスと論理パスへ解決します。
		/// @param parentPhysicalPath 親ShaderSourceの物理パス。
		/// @param parentMountId 親ShaderSourceのmount ID。
		/// @param reference include参照。
		/// @return mount内の候補。無効参照またはmount外脱出時はstd::nullopt。
		[[nodiscard]] std::optional<ResolvedShaderInclude> Resolve(
			const Path& parentPhysicalPath,
			std::string_view parentMountId,
			const ShaderIncludeReference& reference
		) const;

	private:
		const ContentPathResolver& mContentPathResolver;
	};
}
