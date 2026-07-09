#include "ShaderIncludeResolver.h"

#include "core/content/ContentPathResolver.h"

namespace Unnamed {
	ShaderIncludeResolver::ShaderIncludeResolver(
		const ContentPathResolver& contentPathResolver
	) : mContentPathResolver(contentPathResolver) {
	}

	std::optional<ResolvedShaderInclude> ShaderIncludeResolver::Resolve(
		const Path&                   parentPhysicalPath,
		const std::string_view        parentMountId,
		const ShaderIncludeReference& reference
	) const {
		if (parentPhysicalPath.IsEmpty() || parentMountId.empty() ||
		    reference.path.empty()) {
			return std::nullopt;
		}

		std::optional<ResolvedContentFile> candidate;
		if (reference.kind == ShaderIncludeKind::SourceRelative) {
			const Path physicalPath = (
				parentPhysicalPath.ParentPath() / Path(reference.path)
			).LexicallyNormal();
			candidate = mContentPathResolver.DescribePathFromMount(
				parentMountId, physicalPath
			);
		} else {
			const std::optional<VirtualPath> virtualPath =
				VirtualPath::ParseContentReference(reference.path);
			if (!virtualPath.has_value()) {
				return std::nullopt;
			}
			candidate = mContentPathResolver.BuildFileCandidateFromMount(
				parentMountId, *virtualPath
			);
		}

		if (!candidate.has_value()) {
			return std::nullopt;
		}
		return ResolvedShaderInclude{
			.reference    = reference,
			.virtualPath  = candidate->virtualPath,
			.physicalPath = candidate->resolvedPath,
			.mountId      = candidate->mountId,
		};
	}
}
