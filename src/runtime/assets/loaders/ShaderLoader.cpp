#include <pch.h>

//-----------------------------------------------------------------------------

#include "ShaderLoader.h"

#include <filesystem>
#include <fstream>
#include <regex>

namespace Unnamed {
	constexpr std::string_view kChannel = "ShaderLoader";

	bool ShaderLoader::CanLoad(
		std::string_view path, UASSET_TYPE* outType
	) const {
		std::string ext = StrUtil::ToLowerExt(path);
		bool        ok  = (
			ext == ".hlsl" ||
			ext == ".hlsli" ||
			ext == ".dxil"
		);

		if (outType) {
			*outType = ok ? UASSET_TYPE::SHADER : UASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult ShaderLoader::Load(const std::string& path) {
		LoadResult      result = {};
		ShaderAssetData s      = {};
		s.sourcePath           = path;

		const std::string ext = StrUtil::ToLowerExt(path);
		if (ext == ".hlsl" || ext == ".hlsli") {
			std::ifstream ifs(path, std::ios::binary);
			if (!ifs) {
				Error(kChannel, "Failed to open file: {}", path);
				return result;
			}
			std::string txt((std::istreambuf_iterator(ifs)),
			                std::istreambuf_iterator<char>());
			s.hlsl = std::move(txt);

			// #include 解析して依存に登録
			for (auto inc : ExtractIncludes(s.hlsl)) {
				std::filesystem::path base = std::filesystem::path(path).
					parent_path();
				std::filesystem::path incp = (base / inc).lexically_normal();
				if (std::filesystem::exists(incp)) {
					AssetID dep = mAssetManager ?
						              mAssetManager->LoadFromFile(
							              incp.string(), UASSET_TYPE::SHADER) :
						              kInvalidAssetID;
					if (dep != kInvalidAssetID) {
						result.dependencies.emplace_back(dep);
					}
				}
			}
		} else {
			std::ifstream ifs(path, std::ios::binary);
			if (!ifs) {
				Error(kChannel, "Failed to open file: {}", path);
				return result;
			}
			std::vector<uint8_t> bin((std::istreambuf_iterator<char>(ifs)),
			                         std::istreambuf_iterator<char>());

			const std::string stage = GuessStageKeyFromFilename(path);
			if (!stage.empty()) {
				s.blobs[stage] = std::move(bin);
			} else {
				// 謎のステージの場合は ANY に突っ込む
				s.blobs["ANY"] = std::move(bin);
			}
		}

		result.payload     = std::move(s);
		result.resolveName = std::filesystem::path(path).filename().string();
		if (std::error_code ec; std::filesystem::exists(path, ec)) {
			result.stamp.sizeInBytes = std::filesystem::file_size(path, ec);
		}
		return result;
	}

	std::string ShaderLoader::GuessStageKeyFromFilename(
		const std::string& path
	) {
		const std::string lower = StrUtil::ToLowerCase(path);
		const struct {
			const char* key;
			const char* token;
		} stages[] = {
				{"VS", ".vs."}, {"PS", ".ps."}, {"GS", ".gs."}, {"CS", ".cs."},
				{"VS", "_vs."}, {"PS", "_ps."}, {"GS", "_gs."}, {"CS", "_cs."},
				{"VS", "-vs."}, {"PS", "-ps."}, {"GS", "-gs."}, {"CS", "-cs."},
			};
		for (auto& s : stages) {
			if (lower.find(s.token) != std::string::npos) {
				return s.key;
			}
		}
		if (lower.rfind("vs.cso") != std::string::npos || lower.rfind("vs.dxil")
			!= std::string::npos)
			return "VS";
		if (lower.rfind("ps.cso") != std::string::npos || lower.rfind("ps.dxil")
			!= std::string::npos)
			return "PS";
		if (lower.rfind("gs.cso") != std::string::npos || lower.rfind("gs.dxil")
			!= std::string::npos)
			return "GS";
		if (lower.rfind("cs.cso") != std::string::npos || lower.rfind("cs.dxil")
			!= std::string::npos)
			return "CS";

		return {};
	}

	std::vector<std::string> ShaderLoader::ExtractIncludes(
		const std::string& src
	) {
		static const std::regex  kRe(R"(#\s*include\s*\"([^\"]+)\")");
		std::vector<std::string> out;
		for (std::sregex_iterator it(src.begin(), src.end(), kRe), end; it !=
		     end
		     ; ++it) {
			out.emplace_back((*it)[1].str());
		}
		return out;
	}
}
