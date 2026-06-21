#include "DxcShaderCompiler.h"

#include <filesystem>
#include <fstream>
#include <iterator>

#include "core/string/StrUtil.h"

#include "engine/unnamed/subsystem/console/Log.h"

#pragma comment(lib, "dxcompiler.lib")

namespace Unnamed::Rhi {
	DxcShaderCompiler::DxcShaderCompiler() {
	}

	bool DxcShaderCompiler::Initialize() {
		if (FAILED(
			DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(mUtils.
				ReleaseAndGetAddressOf()))
		))
			return false;
		if (FAILED(
			DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(mCompiler.
				ReleaseAndGetAddressOf()))
		))
			return false;

		if (FAILED(
			mUtils->CreateDefaultIncludeHandler(mIncludeHandler.
				ReleaseAndGetAddressOf())
		))
			return false;
		return true;
	}

	static bool WriteBinaryFile(
		const std::wstring& path, const void* data, size_t size
	) {
		std::ofstream ofs(path, std::ios::binary);
		if (!ofs)
			return false;
		ofs.write(
			reinterpret_cast<const char*>(data),
			static_cast<std::streamsize>(size)
		);
		return true;
	}

	bool DxcShaderCompiler::CompileToFileDXIL(
		const std::wstring& sourcePath, const std::wstring& entryPoint,
		const std::wstring& targetProfile,
		const std::vector<std::wstring>& includeDirs,
		const std::vector<std::wstring>& extraArgs,
		const std::wstring& outputPath
	) {
		return CompileInternal(
			sourcePath, entryPoint, targetProfile, includeDirs, extraArgs,
			outputPath
		);
	}

	bool DxcShaderCompiler::CompileInternal(
		const std::wstring& sourcePath, const std::wstring& entryPoint,
		const std::wstring& targetProfile,
		const std::vector<std::wstring>& includeDirs,
		const std::vector<std::wstring>& extraArgs,
		const std::wstring& outputPath
	) {
		if (!mUtils || !mCompiler)
			return false;

		std::ifstream sourceFile(sourcePath, std::ios::binary);
		if (!sourceFile) {
			Error("Shader", "Failed to open shader source file.");
			return false;
		}
		std::string sourceCode(
			(std::istreambuf_iterator<char>(sourceFile)),
			std::istreambuf_iterator<char>()
		);
		if (sourceFile.bad()) {
			Error("Shader", "Failed to read shader source file.");
			return false;
		}

		DxcBuffer src{};
		src.Ptr      = sourceCode.data();
		src.Size     = sourceCode.size();
		src.Encoding = DXC_CP_UTF8;

		// 引数は LPCWSTR 配列。寿命を保つため wstring を別ベクタで保持
		std::vector<std::wstring> owned;
		owned.reserve(16 + includeDirs.size() * 2 + extraArgs.size());

		auto Push = [&](const std::wstring& s) {
			owned.emplace_back(s);
		};

		// 必須
		// DXC は先頭にソースパスを渡す形式を前提にする実装があるため明示する
		Push(sourcePath);
		Push(L"-E");
		Push(entryPoint);
		Push(L"-T");
		Push(targetProfile);

		// 便利設定（好みで）
		Push(L"-WX");           // 警告をエラー
		Push(L"-Zi");           // デバッグ情報
		Push(L"-Qembed_debug"); // DXILにデバッグ埋め込み
		Push(L"-O3");           // 最適化
		Push(L"-Zpr");          // メモリレイアウトは行優先（row-major）

		// include
		for (const auto& dir : includeDirs) {
			Push(L"-I");
			Push(dir);
		}

		// 追加引数
		for (const auto& a : extraArgs) {
			Push(a);
		}

		std::vector<LPCWSTR> argv;
		argv.reserve(owned.size());
		for (auto& s : owned)
			argv.emplace_back(s.c_str());

		Microsoft::WRL::ComPtr<IDxcResult> result;
		if (FAILED(
			mCompiler->Compile(&src, argv.data(), static_cast<uint32_t>(argv.
					size()),
				mIncludeHandler.Get(), IID_PPV_ARGS(result.
					ReleaseAndGetAddressOf()))
		)) {
			return false;
		}

		HRESULT status = S_OK;
		result->GetStatus(&status);

		// エラー/警告ログ
		std::string_view                     diagnostics = {};
		Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
		if (SUCCEEDED(
			result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.
				ReleaseAndGetAddressOf()), nullptr)
		)) {
			if (errors && errors->GetStringLength() > 0) {
				const char* message = errors->GetStringPointer();
				diagnostics         = message ?
					              std::string_view(message) :
					              std::string_view{};
			}
		}

		if (FAILED(status)) {
			if (!diagnostics.empty()) {
				Error("Shader", "{}", diagnostics);
			} else {
				Error(
					"Shader",
					"Compile failed with status=0x{:08X} for '{}'",
					static_cast<uint32_t>(status),
					StrUtil::ToString(sourcePath)
				);
			}
			return false;
		}
		if (!diagnostics.empty()) {
			Warning("Shader", "{}", diagnostics);
		}

		Microsoft::WRL::ComPtr<IDxcBlob> objectBlob;
		if (FAILED(
			result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(objectBlob.
				ReleaseAndGetAddressOf()), nullptr)
		)) {
			return false;
		}

		const auto outputDirectory = std::filesystem::path(outputPath).
			parent_path();
		if (!outputDirectory.empty()) {
			std::filesystem::create_directories(outputDirectory);
		}

		Msg(
			"Shader", "Compile Succeeded! {} to {}",
			StrUtil::ToString(sourcePath), StrUtil::ToString(outputPath)
		);

		return WriteBinaryFile(
			outputPath, objectBlob->GetBufferPointer(),
			objectBlob->GetBufferSize()
		);
	}
}
