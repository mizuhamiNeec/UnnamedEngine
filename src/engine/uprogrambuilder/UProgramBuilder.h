#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace Unnamed {
	/// @brief プログラムリソース構造体
	struct ProgramResources {
		std::vector<std::string>             textures;
		std::unordered_map<std::string, int> paramSlots;
	};

	/// @brief 生成されたプログラム構造体
	struct GeneratedProgram {
		std::string      hlsl; // 完成したものがこちらになりまーす!
		std::string      vsEntry = "VSMain";
		std::string      psEntry = "PSMain";
		ProgramResources resources;
		uint64_t         hash = 0;
	};

	/// @brief プログラムビルド入力構造体
	struct ProgramBuildInput {
		std::string bodyPath; // 実装
		std::string metaPath; // JSON
		std::vector<std::string> defines;
		std::string abiInclude = "./content/core/shaders/MaterialABI.hlsli";
	};

	/// @brief プログラムビルダークラス
	class UProgramBuilder {
	public:
		static bool BuildFromBody(
			const ProgramBuildInput& input,
			GeneratedProgram&        out,
			std::string*             errMsg = nullptr
		);
	};
}
