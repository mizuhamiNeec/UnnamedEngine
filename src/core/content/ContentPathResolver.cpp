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
