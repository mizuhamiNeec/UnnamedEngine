#include "BasicBinaryLoader.h"

#include <filesystem>
#include <fstream>

namespace Unnamed {
	bool BasicBinaryLoader::CanLoad(
		const std::string_view path,
		UASSET_TYPE*     outType
	) const {
		std::string ext;
		{
			auto p = std::filesystem::path(std::string(path));
			ext    = p.extension().string();
			for (auto& c : ext) {
				c = static_cast<std::string::value_type>(std::tolower(c));
			}
		}
		auto type = UASSET_TYPE::UNKNOWN;
		if (ext == ".dds" || ext == ".png") {
			type = UASSET_TYPE::TEXTURE;
		} else if (ext == ".hlsl") {
			type = UASSET_TYPE::SHADER;
		} else if (ext == ".mat") {
			type = UASSET_TYPE::MATERIAL;
		} else if (ext == ".obj" || ext == ".gltf" || ext == ".glb") {
			type = UASSET_TYPE::MESH;
		} else if (ext == ".wav" || ext == ".ogg") {
			type = UASSET_TYPE::SOUND;
		}

		if (outType) {
			*outType = type;
		}
		return type != UASSET_TYPE::UNKNOWN;
	}

	LoadResult BasicBinaryLoader::Load(
		const std::string& path
	) {
		LoadResult    result = {};
		std::ifstream ifs(path, std::ios::binary);
		if (!ifs) {
			return result;
		}

		std::vector<uint8_t> bytes(
			(std::istreambuf_iterator<char>(ifs)),
			std::istreambuf_iterator<char>()
		);

		UASSET_TYPE type;
		CanLoad(path, &type);
		result.stamp.sizeInBytes = bytes.size();
		result.resolveName = std::filesystem::path(path).filename().string();

		switch (type) {
		case UASSET_TYPE::UNKNOWN:
			break;
		case UASSET_TYPE::TEXTURE: {
			TextureAssetData data;
			data.bytes      = std::move(bytes);
			data.sourcePath = path;
			result.payload  = std::move(data);
			break;
		}
		case UASSET_TYPE::SHADER: {
			ShaderAssetData data;
			data.hlsl.assign(
				reinterpret_cast<const char*>(bytes.data()),
				bytes.size()
			);
			data.sourcePath = path;
			result.payload  = std::move(data);
			break;
		}
		case UASSET_TYPE::MATERIAL: {
			MaterialAssetData data;
			data.sourcePath = path;
			// TODO: json パース
			// result.dependencies.push_back();
			result.payload = std::move(data);
			break;
		}
		case UASSET_TYPE::MESH: {
			MeshAssetData data;
			data.bytes      = std::move(bytes);
			data.sourcePath = path;
			result.payload  = std::move(data);
			break;
		}
		case UASSET_TYPE::SOUND: {
			SoundAssetData data;
			data.bytes      = std::move(bytes);
			data.sourcePath = path;
			result.payload  = std::move(data);
			break;
		}
		default: break;
		}

		return result;
	}
}
