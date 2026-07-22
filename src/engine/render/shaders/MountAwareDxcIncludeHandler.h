#pragma once

#include <atomic>

#include <dxcapi.h>
#include <wrl/client.h>

#include "ShaderCompileUnit.h"

namespace Unnamed::Render {
	/// @brief 事前解決済みShaderCompileUnitだけをDXCへ公開するinclude handlerです。
	class MountAwareDxcIncludeHandler final : public IDxcIncludeHandler {
	public:
		/// @brief handlerを生成します。
		/// @param dxcUtils UTF-8 blob生成に使用するDXC utility。
		/// @param compileUnit 事前解決済みinclude table。
		/// @return COM参照を保持するhandler。
		[[nodiscard]] static Microsoft::WRL::ComPtr<IDxcIncludeHandler> Create(
			IDxcUtils& dxcUtils,
			const ShaderCompileUnit& compileUnit
		);

		HRESULT STDMETHODCALLTYPE QueryInterface(
			REFIID interfaceId, void** object
		) override;
		ULONG STDMETHODCALLTYPE AddRef() override;
		ULONG STDMETHODCALLTYPE Release() override;
		HRESULT STDMETHODCALLTYPE LoadSource(
			LPCWSTR fileName, IDxcBlob** includeSource
		) override;

	private:
		MountAwareDxcIncludeHandler(
			IDxcUtils& dxcUtils,
			const ShaderCompileUnit& compileUnit
		);

		std::atomic<ULONG> mReferenceCount = 1;
		Microsoft::WRL::ComPtr<IDxcUtils> mDxcUtils;
		const ShaderCompileUnit&          mCompileUnit;
	};
}
