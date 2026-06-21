#include "AssetType.h"

#include <algorithm>
#include <string_view>

#include "core/filesystem/Path.h"

std::string Unnamed::ToString(const ASSET_TYPE e) {
	switch (e) {
		case ASSET_TYPE::UNKNOWN: return "UNKNOWN";
		case ASSET_TYPE::TEXTURE: return "TEXTURE";
		case ASSET_TYPE::SHADER_SOURCE: return "SHADER_SOURCE";
		case ASSET_TYPE::MATERIAL: return "MATERIAL";
		case ASSET_TYPE::MESH: return "MESH";
		case ASSET_TYPE::SOUND: return "SOUND";
		case ASSET_TYPE::RAW_FILE: return "RAW_FILE";
		case ASSET_TYPE::SHADER_PROGRAM: return "SHADER_PROGRAM";
		case ASSET_TYPE::MATERIAL_INSTANCE: return "MATERIAL_INSTANCE";
		case ASSET_TYPE::POST_FX_CHAIN: return "POST_FX_CHAIN";
		case ASSET_TYPE::UI_DOCUMENT: return "UI_DOCUMENT";
		case ASSET_TYPE::EVENT_PRESENTATION: return "EVENT_PRESENTATION";
		case ASSET_TYPE::SEQUENCE: return "SEQUENCE";
		case ASSET_TYPE::EDITOR_GUI: return "EDITOR_GUI";
		default: return "unknown";
	}
}

Unnamed::ASSET_TYPE Unnamed::GuessAssetTypeFromPath(
	const Path& path
) {
	auto ToLower = [](std::string value) {
		std::ranges::transform(
			value,
			value.begin(),
			[](const unsigned char c) {
				return static_cast<char>(std::tolower(c));
			}
		);
		return value;
	};
	const std::string lower = ToLower(std::string(path));

	auto EndsWith = [&](const std::string_view suffix) {
		return lower.size() >= suffix.size() &&
		       lower.ends_with(suffix);
	};

	if (
		EndsWith(".png") || EndsWith(".jpg") || EndsWith(".jpeg") ||
		EndsWith(".dds") || EndsWith(".bmp") || EndsWith(".tga") ||
		EndsWith(".tif") || EndsWith(".tiff") || EndsWith(".hdr")
	) {
		return ASSET_TYPE::TEXTURE;
	}

	if (
		EndsWith(".wav") || EndsWith(".ogg") || EndsWith(".mp3") ||
		EndsWith(".flac")
	) {
		return ASSET_TYPE::SOUND;
	}

	if (
		EndsWith(".gltf") || EndsWith(".glb") || EndsWith(".fbx") ||
		EndsWith(".obj")
	) {
		return ASSET_TYPE::MESH;
	}

	if (EndsWith(".shader.json")) {
		return ASSET_TYPE::SHADER_PROGRAM;
	}

	if (EndsWith(".material.json")) {
		return ASSET_TYPE::MATERIAL;
	}

	if (EndsWith(".matinst.json")) {
		return ASSET_TYPE::MATERIAL_INSTANCE;
	}

	if (EndsWith(".postfx.json")) {
		return ASSET_TYPE::POST_FX_CHAIN;
	}

	if (EndsWith(".ui.json")) {
		return ASSET_TYPE::UI_DOCUMENT;
	}

	if (EndsWith(".event_presentation.json")) {
		return ASSET_TYPE::EVENT_PRESENTATION;
	}

	if (EndsWith(".sequence.json")) {
		return ASSET_TYPE::SEQUENCE;
	}

	if (EndsWith(".hlsl") || EndsWith(".hlsli")) {
		return ASSET_TYPE::SHADER_SOURCE;
	}

	if (EndsWith(".edgui.json")) {
		return ASSET_TYPE::EDITOR_GUI;
	}

	return ASSET_TYPE::UNKNOWN;
}
