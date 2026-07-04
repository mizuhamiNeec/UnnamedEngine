#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/filesystem/Path.h"
#include "core/filesystem/VirtualPath.h"

namespace Unnamed {
	/// @brief コンテンツディレクトリのマウント情報
	struct ContentDirectoryMount final {
		std::string id;
		Path        rootPath;
		int         priority = 0;

		uint64_t sequence = 0; // マウント順
	};

	/// @brief 論理コンテンツパスの物理パスへの解決結果
	struct ResolvedContentFile final {
		VirtualPath virtualPath;
		Path        resolvedPath;

		std::string mountId;
		int         mountPriority = 0;
	};

	/// @brief 仮想パスをマウント済みのコンテンツディレクトリから解決するためのクラス。
	/// @details 同じ優先度の場合は、後からマウントされたディレクトリを優先します。
	class ContentPathResolver {
	public:
		/// @brief コンテンツディレクトリをマウントします。
		/// @param mountId マウントID。
		/// @param rootPath マウントするコンテンツディレクトリのルートパス。
		/// @param priority マウントの優先度。大きいほど優先されます。
		[[nodiscard]] bool MountDirectory(
			std::string mountId, const Path& rootPath, int priority
		);

		/// @brief 優先度に従ってファイルを解決します。
		/// @param virtualPath 仮想パス。
		/// @return 解決結果。解決できなかった場合は std::nulloptinal を返します。
		[[nodiscard]] std::optional<ResolvedContentFile> ResolveFile(
			const VirtualPath& virtualPath
		) const;

		/// @brief 指定されたマウントからファイルを解決します。
		/// @param mountId マウントID。
		/// @param virtualPath 仮想パス。
		/// @return 解決結果。解決できなかった場合は std::nulloptinal を返します。
		[[nodiscard]] std::optional<ResolvedContentFile> ResolveFileFromMount(
			std::string_view   mountId,
			const VirtualPath& virtualPath
		) const;

		/// @brief 指定されたマウントが存在するかどうかを確認します。
		/// @param mountId マウントID。
		/// @return 存在する場合は true、存在しない場合は false。
		[[nodiscard]] bool HasMount(std::string_view mountId) const noexcept;

		/// @brief 解決済み物理パスを包含するmount IDを取得します。
		/// @param resolvedPath 解決済みの物理パス。
		/// @return 所属mount ID。mount外の場合はstd::nullopt。
		[[nodiscard]] std::optional<std::string> FindMountIdForResolvedPath(
			const Path& resolvedPath
		) const;

		/// @brief マウントされているコンテンツディレクトリの情報を取得します。
		/// @return マウントされているコンテンツディレクトリの情報のリスト。
		[[nodiscard]]
		const std::vector<ContentDirectoryMount>& GetMounts() const noexcept;

	private:
		/// @brief 指定されたマウントと仮想パスから候補となる物理パスを構築します。
		/// @param mount マウント情報。
		/// @param virtualPath 仮想パス。
		/// @return 候補となる物理パス。
		[[nodiscard]]
		static Path BuildCandidatePath(
			const ContentDirectoryMount& mount, const VirtualPath& virtualPath
		);

		/// @brief マウント情報を優先度順にソートします。
		void SortMounts();

		std::vector<ContentDirectoryMount> mMounts;
		uint64_t                           mNextSequence = 0;
	};
}
