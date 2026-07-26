#pragma once
#include <string>

#ifdef _DEBUG
namespace Unnamed {
	class ConsoleSystem;

	/// @brief ImGuizmoConfigLoaderは、ImGuizmo設定ファイルを読み込み、コンソール変数へ反映します
	class ImGuizmoConfigLoader {
	public:
		ImGuizmoConfigLoader(std::string configPath);
	};
}
#endif
