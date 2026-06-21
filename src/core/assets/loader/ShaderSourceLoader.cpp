#include "ShaderSourceLoader.h"
#include "core/filesystem/Path.h"

#include <cctype>
#include <filesystem>

#include "core/assets/AssetManager.h"

#include "core/string/StrUtil.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	static constexpr std::string_view kChannel                 = "ShaderSrcLdr";
	static constexpr std::string_view kSupportedHlslExtension  = ".hlsl";
	static constexpr std::string_view kSupportedHlsliExtension = ".hlsli";

	ShaderSourceLoader::ShaderSourceLoader(AssetManager* assetManager) :
		mAssetManager(assetManager) {}

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
		LoadResult r = {};

		ShaderSourceAssetData data = {};
		data.path                  = path.LexicallyNormal();

		std::string text;
		if (!StrUtil::ReadFileToString(path, text)) {
			Error(kChannel, "シェーダーソースの読み込みに失敗しました: {}", path);
			return r;
		}
		data.includePaths = ParseIncludes(text);

		const Path baseDir = path.ParentPath();

		// 依存関係の解決
		for (const auto& include : data.includePaths) {
			auto includePath = Path(include);
			if (includePath.IsRelative()) {
				includePath = (baseDir / includePath).LexicallyNormal();
			}
			const AssetID depId = mAssetManager->LoadFromFile(
				includePath.LexicallyNormal(), ASSET_TYPE::SHADER_SOURCE
			);
			if (depId != kInvalidAssetID) {
				r.dependencies.emplace_back(depId);
			}
		}

		r.payload     = std::move(data);
		r.resolveName = Path::ToUtf8String(path.FileName());
		if (std::error_code ec; std::filesystem::exists(path.Native(), ec)) {
			r.stamp.sizeInBytes = std::filesystem::file_size(
				path.Native(), ec
			);
		}

		return r;
	}

	std::vector<std::string> ShaderSourceLoader::ParseIncludes(
		const std::string& text
	) {
		std::vector<std::string> result;
		size_t                   lineBegin = 0;
		static constexpr std::string_view kIncludeToken = "#include";

		while (lineBegin < text.size()) {
			const size_t lineEnd = text.find('\n', lineBegin);
			const size_t lineSize = (lineEnd == std::string::npos) ?
				                        (text.size() - lineBegin) :
				                        (lineEnd - lineBegin);
			const std::string_view line(text.data() + lineBegin, lineSize);

			const size_t includePos = line.find(kIncludeToken);
			if (includePos != std::string_view::npos) {
				size_t cursor = includePos + kIncludeToken.size();
				while (
					cursor < line.size() &&
					std::isspace(static_cast<unsigned char>(line[cursor]))
				) {
					++cursor;
				}

				if (cursor < line.size()) {
					const char openDelimiter = line[cursor];
					char       closeDelimiter = '\0';
					if (openDelimiter == '"') {
						closeDelimiter = '"';
					} else if (openDelimiter == '<') {
						closeDelimiter = '>';
					}

					if (closeDelimiter != '\0') {
						const size_t closePos = line.find(closeDelimiter, cursor + 1);
						if (closePos != std::string_view::npos && closePos > cursor + 1) {
							result.emplace_back(
								line.substr(
									cursor + 1,
									closePos - (cursor + 1)
								)
							);
						}
					}
				}
			}

			if (lineEnd == std::string::npos) {
				break;
			}
			lineBegin = lineEnd + 1;
		}
		return result;
	}
}
