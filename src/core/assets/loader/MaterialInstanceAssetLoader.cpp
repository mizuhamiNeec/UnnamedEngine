#include "MaterialInstanceAssetLoader.h"
#include "core/filesystem/Path.h"

#include <filesystem>

#include "core/assets/AssetManager.h"
#include "core/assets/types/MaterialInstanceAssetData.h"
#include "core/io/json/JsonReader.h"

#include "core/string/StrUtil.h"

namespace Unnamed {
	namespace {
		/// @brief マテリアルインスタンスアセットのパスか?
		/// @param path パス
		/// @return マテリアルインスタンスアセットのパスならtrue
		bool IsMaterialInstancePath(const Path& path) {
			return StrUtil::ToLowerCase(path.ToGenericUtf8()).ends_with(
				".matinst.json"
			);
		}
	}

	MaterialInstanceAssetLoader::MaterialInstanceAssetLoader(
		AssetManager* assetManager
	) : mAssetManager(assetManager) {}

	bool MaterialInstanceAssetLoader::CanLoad(
		const Path& path, ASSET_TYPE* outType
	) const {
		// 拡張子ベースで判定。厳密なファイル存在チェックはLoad()に任せる。
		const bool ok = IsMaterialInstancePath(path);
		if (outType) {
			*outType = ok ? ASSET_TYPE::MATERIAL_INSTANCE : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult MaterialInstanceAssetLoader::Load(const Path& path) {
		LoadResult       result = {};
		const JsonReader root(path);
		if (!root.Valid()) {
			return result;
		}

		const Path full    = path.LexicallyNormal();
		const Path baseDir = full.ParentPath();

		// "name" フィールドがあればそれを、なければファイル名をアセット名とする。
		MaterialInstanceAssetData data = {};
		data.name = root.Read<std::string>("name").value_or(
			Path::ToUtf8String(full.FileName())
		);

		// "material" フィールドがあればマテリアルアセットを読み込む。
		if (const auto materialPath = root.Read<std::string>("material");
			materialPath.has_value() && !materialPath->empty()) {
			data.materialPath = Path::ResolveRelativePath(
				baseDir.Native(),
				*materialPath
			);
			data.materialId   = mAssetManager->LoadFromFile(
				data.materialPath, ASSET_TYPE::MATERIAL
			);
			if (data.materialId != kInvalidAssetID) {
				result.dependencies.emplace_back(data.materialId);
			}
		}

		// "textures" フィールドがあればテクスチャオーバーライドを読み込む。
		const JsonReader textures = root["textures"];
		if (textures.Valid() && textures.IsObject()) {
			textures.ForEachObject(
				[&](
				const std::string& slot, const JsonReader& texturePathNode
			) {
					if (!texturePathNode.IsString()) {
						return;
					}
					const Path texturePath = Path::ResolveRelativePath(
						baseDir.Native(),
						texturePathNode.GetString()
					);
					data.textureOverrides[slot] = texturePath;

					const AssetID textureDep = mAssetManager->LoadFromFile(
						texturePath, ASSET_TYPE::TEXTURE
					);
					if (textureDep != kInvalidAssetID) {
						result.dependencies.emplace_back(textureDep);
					}
				}
			);
		}

		// "scalars" フィールドがあればスカラーオーバーライドを読み込む。
		const JsonReader scalars = root["scalars"];
		if (scalars.Valid() && scalars.IsObject()) {
			scalars.ForEachObject(
				[&data](const std::string& k, const JsonReader& v) {
					if (!v.IsNumber()) {
						return;
					}
					data.scalarOverrides[k] = v.GetFloat();
				}
			);
		}

		// "vectors" フィールドがあればベクターオーバーライドを読み込む。
		const JsonReader vectors = root["vectors"];
		if (vectors.Valid() && vectors.IsObject()) {
			vectors.ForEachObject(
				[&data](const std::string& k, const JsonReader& v) {
					data.vectorOverrides[k] = v.GetVec4(Vec4(0, 0, 0, 0));
				}
			);
		}

		// payloadにデータをセット
		result.payload = std::move(data);

		// アセット名が指定されていない場合は、ファイル名から拡張子を除いたものをアセット名とする。
		result.resolveName = Path::ToUtf8String(full.Stem().Stem());

		std::error_code ec;
		if (std::filesystem::exists(path.Native(), ec)) {
			result.stamp.sizeInBytes = std::filesystem::file_size(
				path.Native(), ec
			);
		}
		return result;
	}
}
