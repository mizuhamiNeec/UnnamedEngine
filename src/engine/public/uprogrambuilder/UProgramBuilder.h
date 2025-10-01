#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace Unnamed {
	struct ProgramResources {
		std::vector<std::string>             textures;
		std::unordered_map<std::string, int> paramSlots;
	};

	struct GeneratedProgram {
		std::string      hlsl; // 完成したものがこちらになりまーす!
		std::string      vsEntry = "VSMain";
		std::string      psEntry = "PSMain";
		ProgramResources resources;
		uint64_t         hash = 0;
	};

	struct ProgramBuildInput {
		std::string bodyPath; // 実装
		std::string metaPath; // JSON
		std::vector<std::string> defines;
		std::string abiInclude = "./resources/shaders/MaterialABI.hlsli";
	};

	class UProgramBuilder {
	public:
		static bool BuildFromBody(
			const ProgramBuildInput& input,
			GeneratedProgram&        out,
			std::string*             errMsg = nullptr
		);
	};
}
