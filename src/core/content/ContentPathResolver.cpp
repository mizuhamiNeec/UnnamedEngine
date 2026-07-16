#include "pch.h"
#include "ContentPathResolver.h"

namespace Unnamed {
	bool ContentPathResolver::MountDirectory(
		std::string mountId, const Path& rootPath, const int priority
	) {
		if (mountId.empty()) {
			return false;
		}
		if (rootPath.IsEmpty()) {
			return false;
		}
		if (!rootPath.IsAbsolute()) {
			return false;
		}
		if (!rootPath.IsDirectory()) {
			return false;
		}
		if (HasMount(mountId)) {
			return false;
		}

		// 相対パスの解釈を呼び出し元の作業ディレクトリに委ねない
		ContentDirectoryMount mount{
			.id       = std::move(mountId),
			.rootPath = rootPath.LexicallyNormal(),
			.priority = priority,
			.sequence = mNextSequence,
		};

		mMounts.emplace_back(std::move(mount));
		++mNextSequence;
		SortMounts();
		return true;
	}

	std::optional<ResolvedContentFile> ContentPathResolver::ResolveFile(
		const VirtualPath& virtualPath
	) const {
		if (virtualPath.IsEmpty()) {
			return std::nullopt;
		}

		// 優先度順に並んだ最初の実在ファイルを採用する
		for (const ContentDirectoryMount& mount : mMounts) {
			const Path candidatePath = BuildCandidatePath(mount, virtualPath);
			if (!candidatePath.IsRegularFile()) {
				continue;
			}

			return ResolvedContentFile{
				.virtualPath   = virtualPath,
				.resolvedPath  = candidatePath,
				.mountId       = mount.id,
				.mountPriority = mount.priority,
			};
		}

		return std::nullopt;
	}

	std::optional<ResolvedContentFile> ContentPathResolver::ResolveFileFromMount(
		const std::string_view mountId,
		const VirtualPath&     virtualPath
	) const {
		if (mountId.empty() || virtualPath.IsEmpty()) {
			return std::nullopt;
		}

		for (const ContentDirectoryMount& mount : mMounts) {
			if (mount.id != mountId) {
				continue;
			}

			const Path candidatePath = BuildCandidatePath(mount, virtualPath);
			if (!candidatePath.IsRegularFile()) {
				return std::nullopt;
			}

			return ResolvedContentFile{
				.virtualPath   = virtualPath,
				.resolvedPath  = candidatePath,
				.mountId       = mount.id,
				.mountPriority = mount.priority,
			};
		}

		return std::nullopt;
	}

	std::optional<ResolvedContentFile>
	ContentPathResolver::BuildFileCandidateFromMount(
		const std::string_view mountId,
		const VirtualPath&     virtualPath
	) const {
		if (mountId.empty() || virtualPath.IsEmpty()) {
			return std::nullopt;
		}

		for (const ContentDirectoryMount& mount : mMounts) {
			if (mount.id == mountId) {
				return ResolvedContentFile{
					.virtualPath   = virtualPath,
					.resolvedPath  = BuildCandidatePath(mount, virtualPath),
					.mountId       = mount.id,
					.mountPriority = mount.priority,
				};
			}
		}
		return std::nullopt;
	}

	std::optional<ResolvedContentFile> ContentPathResolver::DescribePathFromMount(
		const std::string_view mountId,
		const Path&            physicalPath
	) const {
		if (mountId.empty() || physicalPath.IsEmpty() ||
		    !physicalPath.IsAbsolute()) {
			return std::nullopt;
		}

		for (const ContentDirectoryMount& mount : mMounts) {
			if (mount.id != mountId) {
				continue;
			}

			const std::filesystem::path relative = physicalPath.LexicallyNormal().
				Native().lexically_relative(mount.rootPath.Native());
			if (relative.empty() || relative.is_absolute()) {
				return std::nullopt;
			}
			// マウント外のパスを仮想パスとして再公開しない
			for (const auto& component : relative) {
				if (component == "..") {
					return std::nullopt;
				}
			}

			const std::optional<VirtualPath> virtualPath =
				VirtualPath::ParseContentReference(
					Path::ToGenericUtf8(relative)
				);
			if (!virtualPath.has_value()) {
				return std::nullopt;
			}
			return ResolvedContentFile{
				.virtualPath   = *virtualPath,
				.resolvedPath  = physicalPath.LexicallyNormal(),
				.mountId       = mount.id,
				.mountPriority = mount.priority,
			};
		}
		return std::nullopt;
	}

	bool ContentPathResolver::HasMount(
		const std::string_view mountId
	) const noexcept {
		for (const ContentDirectoryMount& mount : mMounts) {
			if (mount.id == mountId) {
				return true;
			}
		}

		return false;
	}

	std::optional<std::string>
	ContentPathResolver::FindMountIdForResolvedPath(
		const Path& resolvedPath
	) const {
		if (resolvedPath.IsEmpty() || !resolvedPath.IsAbsolute()) {
			return std::nullopt;
		}

		const std::filesystem::path normalizedPath =
			resolvedPath.LexicallyNormal().Native();
		for (const ContentDirectoryMount& mount : mMounts) {
			std::error_code ec;
			const std::filesystem::path relativePath =
				std::filesystem::relative(
					normalizedPath,
					mount.rootPath.Native(),
					ec
				);
			if (ec || relativePath.empty() || relativePath.is_absolute()) {
				continue;
			}

			const auto firstComponent = relativePath.begin();
			if (
				firstComponent != relativePath.end() &&
				*firstComponent != ".."
			) {
				return mount.id;
			}
		}

		return std::nullopt;
	}

	const std::vector<ContentDirectoryMount>&
	ContentPathResolver::GetMounts() const noexcept {
		return mMounts;
	}

	Path ContentPathResolver::BuildCandidatePath(
		const ContentDirectoryMount& mount, const VirtualPath& virtualPath
	) {
		const auto relativePath = Path(virtualPath.String());
		return (mount.rootPath / relativePath).LexicallyNormal();
	}

	void ContentPathResolver::SortMounts() {
		// 同じ優先度では後から追加したマウントを上書き層として扱う
		std::ranges::sort(
			mMounts,
			[](const ContentDirectoryMount& lhs, const ContentDirectoryMount& rhs
			) {
				if (lhs.priority != rhs.priority) {
					return lhs.priority > rhs.priority;
				}
				return lhs.sequence > rhs.sequence;
			}
		);
	}
}
