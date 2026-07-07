#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "ShaderIncludeTypes.h"

namespace Unnamed {
	/// @brief HLSLソースからinclude directiveを抽出します。
	class ShaderIncludeParser final {
	public:
		/// @brief コメントと行継続を考慮してinclude参照を抽出します。
		/// @details 条件分岐の評価は行わず、構文上存在するincludeを全て返します。
		/// @param source HLSLソーステキスト。
		/// @return 抽出されたinclude参照。
		[[nodiscard]] static std::vector<ShaderIncludeReference> Parse(
			std::string_view source
		);

	private:
		/// @brief コメント除去・行結合済みの1行からincludeを抽出します。
		[[nodiscard]] static std::optional<ShaderIncludeReference> ParseLine(
			std::string_view line
		);
	};
}
