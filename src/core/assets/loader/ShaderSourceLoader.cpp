#include "ShaderSourceLoader.h"
#include "core/filesystem/Path.h"

#include <filesystem>

#include "core/assets/AssetManager.h"
#include "core/assets/shader/ShaderIncludeParser.h"
#include "core/assets/shader/ShaderIncludeResolver.h"
#include "core/content/ContentPathResolver.h"

#include "core/string/StrUtil.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	static constexpr std::string_view kChannel                 = "ShaderSrcLdr";
	static constexpr std::string_view kSupportedHlslExtension  = ".hlsl";
	static constexpr std::string_view kSupportedHlsliExtension = ".hlsli";

	ShaderSourceLoader::ShaderSourceLoader(AssetManager* assetManager) :
		mAssetManager(assetManager) {
	}

	bool ShaderSourceLoader::CanLoad(
		const Path& path, ASSET_TYPE* outType
	) const {
		// outTypeがnullptrならfalseを返す
		if (!outType) {
			return false;
		}
		const std::string extension =
			StrUtil::ToLowerCase(path.Extension().ToGenericUtf8());
		// 拡張子がサポートされているかを確認
		if (
			extension == kSupportedHlslExtension ||
			extension == kSupportedHlsliExtension
		) {
			*outType = ASSET_TYPE::SHADER_SOURCE;
			return true;
		}
		return false;
	}

	LoadResult ShaderSourceLoader::Load(const Path& path) {
		return Load(path, AssetLoadContext{});
	}

	LoadResult ShaderSourceLoader::Load(
		const Path& path, const AssetLoadContext& context
	) {
		LoadResult r = {};

		ShaderSourceAssetData data = {};
		data.path                  = path.LexicallyNormal();
		data.mountId               = std::string(context.resolvedMountId);
		if (data.mountId.empty()) {
			data.mountId = mAssetManager->GetContentPathResolver().
				FindMountIdForResolvedPath(data.path).value_or(std::string{});
		}
		if (!data.mountId.empty()) {
			const auto sourceDescription = mAssetManager->GetContentPathResolver().
				DescribePathFromMount(data.mountId, data.path);
			if (sourceDescription.has_value()) {
				data.virtualPath = sourceDescription->virtualPath;
			}
		}

		std::string text;
		if (!StrUtil::ReadFileToString(path, text)) {
			Error(kChannel, "シェーダーソースの読み込みに失敗しました: {}", path);
			return r;
		}
		data.includeReferences = ShaderIncludeParser::Parse(text);
		data.sourceText        = std::move(text);
		ShaderIncludeResolver includeResolver(
			mAssetManager->GetContentPathResolver()
		);
		bool dependenciesValid = true;

		// 依存関係の解決
		for (const ShaderIncludeReference& reference : data.includeReferences) {
			const std::optional<ResolvedShaderInclude> resolved =
				includeResolver.Resolve(data.path, data.mountId, reference);
			if (!resolved.has_value()) {
				Error(
					kChannel,
					"Invalid or mount-escaping shader include: source='{}' mount='{}' include='{}' kind={}",
					data.path,
					data.mountId,
					reference.path,
					reference.kind == ShaderIncludeKind::SourceRelative ?
						"source-relative" : "mount-root-relative"
				);
				dependenciesValid = false;
				continue;
			}

			ResolvedShaderInclude include = *resolved;
			if (!include.physicalPath.IsRegularFile()) {
				UnresolvedShaderInclude unresolved = {
					.kind                   = reference.kind,
					.path                   = reference.path,
					.mountId                = include.mountId,
					.expectedPhysicalPath   = include.physicalPath,
					.watchedParentDirectory = include.physicalPath.ParentPath(),
				};
				data.unresolvedIncludes.emplace_back(unresolved);
				r.unresolvedShaderIncludes.emplace_back(unresolved);
				r.sourceWatchPaths.emplace_back(
					unresolved.watchedParentDirectory
				);
				Error(
					kChannel,
					"Shader include was not found: source='{}' mount='{}' include='{}' expected='{}'",
					data.path,
					include.mountId,
					reference.path,
					include.physicalPath
				);
				dependenciesValid = false;
				continue;
			}

			const AssetID depId = mAssetManager->LoadAssetFromMount(
				*include.virtualPath,
				include.mountId,
				ASSET_TYPE::SHADER_SOURCE
			);
			if (depId != kInvalidAssetID) {
				include.shaderSourceAssetId = depId;
				data.resolvedIncludes.emplace_back(std::move(include));
				r.dependencies.emplace_back(depId);
			} else {
				const AssetID failedDependency = mAssetManager->FindByPath(
					include.physicalPath
				);
				if (failedDependency != kInvalidAssetID) {
					r.dependencies.emplace_back(failedDependency);
				}
				Error(
					kChannel,
					"Shader include dependency load failed: source='{}' mount='{}' include='{}' physical='{}'",
					data.path,
					include.mountId,
					reference.path,
					include.physicalPath
				);
				dependenciesValid = false;
			}
		}
		if (!dependenciesValid) {
			return r;
		}

		r.payload     = std::move(data);
		r.resolveName = Path::ToUtf8String(path.FileName());
		if (std::error_code ec;
			std::filesystem::exists(path.Native(), ec)) {
			r.stamp.sizeInBytes = std::filesystem::file_size(
				path.Native(), ec
			);
		}

		return r;
	}

}
