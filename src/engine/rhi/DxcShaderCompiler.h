#pragma once
#include <string>
#include <string_view>
#include <vector>

#include <wrl/client.h>

#include <dxcapi.h>

namespace Unnamed::Rhi {
	class DxcShaderCompiler {
	public:
		DxcShaderCompiler();
		bool Initialize();

		bool CompileToFileDXIL(
			std::string_view                 sourceCode,
			const std::wstring&              sourceName,
			const std::wstring&              entryPoint,
			const std::wstring&              targetProfile,
			const std::vector<std::wstring>& extraArgs,
			IDxcIncludeHandler&              includeHandler,
			const std::wstring&              outputPath
		);

		/// @brief 初期化済みDXC utilityを取得します。
		/// @return blob生成に使用できるDXC utility。
		[[nodiscard]] IDxcUtils& GetUtils() const;

	private:
		bool CompileInternal(
			std::string_view                 sourceCode,
			const std::wstring&              sourceName,
			const std::wstring&              entryPoint,
			const std::wstring&              targetProfile,
			const std::vector<std::wstring>& extraArgs,
			IDxcIncludeHandler&              includeHandler,
			const std::wstring&              outputPath
		);

		Microsoft::WRL::ComPtr<IDxcUtils>          mUtils;
		Microsoft::WRL::ComPtr<IDxcCompiler3>      mCompiler;
	};
}
