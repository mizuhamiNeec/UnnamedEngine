#include <pch.h>

//-----------------------------------------------------------------------------
#include "MaterialLoader.h"

#include <fstream>

#include <core/json/JsonReader.h>
#include <core/string/StrUtil.h>

#include <runtime/assets/core/UAssetManager.h>

namespace Unnamed {
	constexpr std::string_view kChannel = "MaterialLoader";

	MaterialLoader::MaterialLoader(UAssetManager* assetManager)
		: mAssetManager(assetManager) {
	}

	bool MaterialLoader::CanLoad(
		const std::string_view path, UASSET_TYPE* outType
	) const {
		const std::string ext = StrUtil::ToLowerExt(path);
		const bool        ok  = (ext == ".mat" || ext == ".json");
		if (outType) {
			*outType = ok ? UASSET_TYPE::MATERIAL : UASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult MaterialLoader::Load(const std::string& path) {
		LoadResult result = {};

		nlohmann::json json;
		{
			std::ifstream ifs(path);
			if (!ifs) {
				Error(kChannel, "Failed to open file: {}", path);
				return result;
			}
			ifs >> json;
		}

		auto jr = JsonReader(json);

		MaterialAssetData m = {};
		m.sourcePath        = path;

		// 名前
		if (json.contains("name")) {
			m.sourcePath = json["name"].get<std::string>();
		}

		// Body/Metaのロード
		if (json.contains("programBody") && json["programBody"].is_string()) {
			const std::string body = json["programBody"].get<std::string>();
			m.programBody          = mAssetManager->LoadFromFile(
				body, UASSET_TYPE::RAWFILE
			);
		}
		if (json.contains("programMeta") && json["programMeta"].is_string()) {
			const std::string meta = json["programMeta"].get<std::string>();
			m.programMeta          = mAssetManager->LoadFromFile(
				meta, UASSET_TYPE::RAWFILE
			);
		}

		// シェーダー
		if (json.contains("shader")) {
			if (json["shader"].is_string()) {
				std::string shaderPath = json["shader"].get<std::string>();
				AssetID     sid        = mAssetManager->LoadFromFile(
					shaderPath, UASSET_TYPE::SHADER
				);
				m.shader = sid;
				result.dependencies.emplace_back(sid);
			} else if (json["shader"].is_object()) {
				const auto& sj = json["shader"];
				if (sj.contains("vs")) {
					AssetID vs = mAssetManager->LoadFromFile(
						sj["vs"].get<std::string>(), UASSET_TYPE::SHADER
					);
					m.shaderVS = vs;
					result.dependencies.emplace_back(vs);
				}
				if (sj.contains("ps")) {
					AssetID ps = mAssetManager->LoadFromFile(
						sj["ps"].get<std::string>(), UASSET_TYPE::SHADER
					);
					m.shaderPS = ps;
					result.dependencies.emplace_back(ps);
				}
				if (sj.contains("gs")) {
					AssetID gs = mAssetManager->LoadFromFile(
						sj["gs"].get<std::string>(), UASSET_TYPE::SHADER
					);
					m.shaderGS = gs;
					result.dependencies.emplace_back(gs);
				}
				if (sj.contains("entryVS")) {
					m.entryVS = sj["entryVS"].get<std::string>();
				}
				if (sj.contains("entryPS")) {
					m.entryPS = sj["entryPS"].get<std::string>();
				}
				if (sj.contains("entryGS")) {
					m.entryGS = sj["entryGS"].get<std::string>();
				}
				if (sj.contains("defines") && sj["defines"].is_array()) {
					for (auto& d : sj["defines"]) {
						m.defines.emplace_back(d.get<std::string>());
					}
				}
			}
		}

		// defines
		if (json.contains("defines") && json["defines"].is_array()) {
			for (auto& v : json["defines"]) {
				if (v.is_string()) {
					m.defines.emplace_back(v.get<std::string>());
				}
			}
		}

		// テクスチャ: { スロット名 : ファイルパス }
		if (json.contains("textures") && json["textures"].is_object()) {
			for (auto& [slot, val] : json["textures"].items()) {
				auto    texPath = val.get<std::string>();
				AssetID tid     = mAssetManager->LoadFromFile(
					texPath, UASSET_TYPE::TEXTURE);
				m.textureSlots[slot] = tid;
				result.dependencies.emplace_back(tid);
			}
		}

		// floats
		if (json.contains("floats") && json["floats"].is_object()) {
			for (auto& [k, v] : json["floats"].items()) {
				m.floatParams[k] = v.get<float>();
				m.params[k]      = MaterialParam{
					ParamType::FLOAT, v.get<float>()
				};
			}
		}

		// float4s
		if (json.contains("float4s") && json["float4s"].is_object()) {
			for (auto& [k,v] : json["float4s"].items()) {
				if (v.is_array() && v.size() == 4) {
					m.float4Params[k] = {
						v[0].get<float>(), v[1].get<float>(),
						v[2].get<float>(), v[3].get<float>()
					};

					m.params[k] = MaterialParam{
						ParamType::FLOAT, {
							v[0].get<float>(), v[1].get<float>(),
							v[2].get<float>(), v[3].get<float>()
						}
					};
				}
			}
		}

		// メタデータ
		result.resolveName = std::filesystem::path(path).filename().string();
		result.stamp.sizeInBytes = std::filesystem::file_size(path);

		// TODO: lastWrite は省略 (必要なら file_time_type を system_clock に変換)
		result.payload = std::move(m);

		return result;
	}
}
