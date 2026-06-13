#include "ShaderSourceLoader.h"

#include <cctype>
#include <filesystem>

#include "core/assets/AssetManager.h"
#include "core/path/PathUtil.h"
#include "core/string/StrUtil.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	static constexpr std::string_view kChannel                 = "ShaderSrcLdr";
	static constexpr std::string_view kSupportedHlslExtension  = ".hlsl";
	static constexpr std::string_view kSupportedHlsliExtension = ".hlsli";

	ShaderSourceLoader::ShaderSourceLoader(AssetManager* assetManager) :
		mAssetManager(assetManager) {}

	bool ShaderSourceLoader::CanLoad(
		const std::string_view path, ASSET_TYPE* outType
	) const {
		// outTypeがnullptrならfalseを返す
		if (!outType) {
			return false;
		}
		// 拡張子がサポートされているかを確認
		if (
			StrUtil::HasExtension(path, kSupportedHlslExtension) ||
			StrUtil::HasExtension(path, kSupportedHlsliExtension)
		) {
			*outType = ASSET_TYPE::SHADER_SOURCE;
			return true;
		}
		return false;
	}

	LoadResult ShaderSourceLoader::Load(const std::string& path) {
		LoadResult r = {};

		ShaderSourceAssetData data = {};
		data.path                  = path;

		std::string text;
		if (!StrUtil::ReadFileToString(path, text)) {
			Error(kChannel, "シェーダーソースの読み込みに失敗しました: {}", path);
			return r;
		}
		data.includePaths = ParseIncludes(text);

		const std::filesystem::path baseDir = Path::FromUtf8(path).parent_path();

		// 依存関係の解決
		for (const auto& include : data.includePaths) {
			std::filesystem::path includePath = Path::FromUtf8(include);
			if (includePath.is_relative()) {
				includePath = baseDir / includePath;
			}
			const std::string depPath = StrUtil::NormalizePath(
				Path::ToGenericUtf8(includePath.lexically_normal())
			);
			const AssetID depId = mAssetManager->LoadFromFile(
				depPath, ASSET_TYPE::SHADER_SOURCE
			);
			if (depId != kInvalidAssetID) {
				r.dependencies.emplace_back(depId);
			}
		}

		r.payload     = std::move(data);
		r.resolveName = Path::ToUtf8String(Path::FromUtf8(path).filename());
		if (std::error_code ec; Path::ExistsUtf8(path, ec)) {
			r.stamp.sizeInBytes = Path::FileSizeUtf8(path, ec);
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
