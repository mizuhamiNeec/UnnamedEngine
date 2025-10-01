#include "UProgramBuilder.h"

#include <fstream>
#include <ios>
#include <sstream>

#include <engine/public/utils/json/JsonReader.h>

namespace Unnamed {
	static std::string ReadTextFile(const std::string& path) {
		const std::ifstream ifs(path, std::ios::binary);
		if (!ifs) {
			return {};
		}
		std::stringstream ss;
		ss << ifs.rdbuf();
		return ss.str();
	}

	static uint64_t FNV1a64(const void* data, const size_t size) {
		const auto p = static_cast<const uint8_t*>(data);
		uint64_t   h = 1469598103934665603ull;
		for (size_t i = 0; i < size; i++) {
			h ^= p[i];
			h *= 1099511628211ull;
		}
		return h;
	}

	bool UProgramBuilder::BuildFromBody(
		const ProgramBuildInput& input,
		GeneratedProgram&        out,
		std::string*             errMsg
	) {
		std::string body = ReadTextFile(input.bodyPath);
		if (body.empty()) {
			if (errMsg) {
				*errMsg = "Body HLSL not found";
				return false;
			}
		}

		ProgramResources r = {};
		std::string      metaRaw;
		if (!input.metaPath.empty()) {
			std::ifstream ifs(input.metaPath);
			if (!ifs) {
				if (errMsg) {
					*errMsg = "Body JSON not found: " + input.metaPath;
					return false;
				}
			}

			nlohmann::json j;
			ifs >> j;
			if (j.contains("textures") && j["textures"].is_array()) {
				for (auto& v : j["textures"]) {
					if (v.is_string()) {
						r.textures.emplace_back(v.get<std::string>());
					}
				}
			}

			if (j.contains("params") && j["params"].is_object()) {
				for (auto it = j["params"].begin(); it != j["params"].end(); ++
				     it) {
					const auto  name = it.key();
					const auto& pobj = it.value();
					if (pobj.contains("slot") && pobj["slot"].
						is_number_integer()) {
						r.paramSlots[name] = pobj["slot"].get<int>();
					}
				}
			}

			metaRaw = ReadTextFile(input.metaPath);
		}

		// パワー! ヤー!
		std::ostringstream ss;
		ss << "#include \"" << input.abiInclude << "\"\n";
		for (auto& d : input.defines) {
			ss << "#define " << d << "\n";
		}

		ss << R"(
struct VSIn { float3 pos:POSITION; float3 col:COLOR; float2 uv:TEXCOORD0; };
struct VSOut {
	float4 pos:SV_POSITION;
	float3 col:COLOR; 
	float2 uv:TEXCOORD0;
};
VSOut VSMain(VSIn i) {
	VSOut o;
	float4 wpos = mul(float4(i.pos,1), gWorld);
	o.pos = mul(wpos, gViewProj);
	o.col = i.col;
	o.uv = i.uv;
	return o;
}
)";

		ss << "\n// ---- Implement From User ----\n\n" << body <<
			"\n\n// ---- User Body End ----\n\n";

		ss << R"(
float4 PSMain(VSOut i) : SV_Target {
	MatIn IN;
	IN.uv = i.uv;
	IN.vertexColor = i.col;
	MatOut OUT;
	ShadeMaterial(IN, OUT);
	return float4(OUT.baseColor, 1);
}
)";

		out.hlsl      = ss.str();
		out.vsEntry   = "VSMain";
		out.psEntry   = "PSMain";
		out.resources = std::move(r);

		uint64_t h = FNV1a64(out.hlsl.data(), out.hlsl.size());
		if (!metaRaw.empty()) {
			h ^= FNV1a64(metaRaw.data(), metaRaw.size());
		}
		for (auto& d : input.defines) {
			h ^= FNV1a64(d.data(), d.size());
		}
		out.hash = h;

		return true;
	}
}
