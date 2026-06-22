#pragma once

namespace Unnamed {
	/// @brief content をルートとした仮想パス
	/// @details OSのパスではなく、各ゲームコンテンツでの仮想パスを表すクラスです。
	/// 区切り文字は常に '/' へ正規化され、絶対パスと親ディレクトリ参照は禁止されます。
	class VirtualPath final {
	public:
		VirtualPath();

		/// @brief UTF-8論理パスを解析します。
		/// @return 解析に成功した場合は VirtualPath を返し、失敗した場合は std::nullopt を返します。
		[[nodiscard]]
		static std::optional<VirtualPath> Parse(std::string_view utf8Path);

		/// @brief UTF-8論理パスを解析します。
		/// @return 解析に成功した場合は VirtualPath を返し、失敗した場合は std::invalid_argument 例外を投げます。
		[[nodiscard]]
		static VirtualPath ParseOrThrow(std::string_view utf8Path);

		/// @brief `/` 区切りの UTF-8論理パスを返します。
		[[nodiscard]]
		const std::string& String() const noexcept;

		/// @brief 空のパスかどうかを返します。
		/// @return パスが空かどうか。
		[[nodiscard]]
		bool IsEmpty() const noexcept;

		[[nodiscard]]
		friend bool operator==(
			const VirtualPath& lhs, const VirtualPath& rhs
		) noexcept = default;

	private:
		explicit VirtualPath(std::string normalizedPath);

		std::string mPath;
	};
}
