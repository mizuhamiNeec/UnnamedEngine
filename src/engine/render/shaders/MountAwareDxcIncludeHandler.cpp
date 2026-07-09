#include "MountAwareDxcIncludeHandler.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Render {
	Microsoft::WRL::ComPtr<IDxcIncludeHandler>
	MountAwareDxcIncludeHandler::Create(
		IDxcUtils& dxcUtils,
		const ShaderCompileUnit& compileUnit
	) {
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> handler;
		handler.Attach(new MountAwareDxcIncludeHandler(dxcUtils, compileUnit));
		return handler;
	}

	HRESULT MountAwareDxcIncludeHandler::QueryInterface(
		REFIID interfaceId, void** object
	) {
		if (!object) {
			return E_POINTER;
		}
		*object = nullptr;
		if (interfaceId == __uuidof(IUnknown) ||
		    interfaceId == __uuidof(IDxcIncludeHandler)) {
			*object = static_cast<IDxcIncludeHandler*>(this);
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}

	ULONG MountAwareDxcIncludeHandler::AddRef() {
		return ++mReferenceCount;
	}

	ULONG MountAwareDxcIncludeHandler::Release() {
		const ULONG referenceCount = --mReferenceCount;
		if (referenceCount == 0) {
			delete this;
		}
		return referenceCount;
	}

	HRESULT MountAwareDxcIncludeHandler::LoadSource(
		const LPCWSTR fileName, IDxcBlob** includeSource
	) {
		if (!fileName || !includeSource) {
			return E_INVALIDARG;
		}
		*includeSource = nullptr;

		std::string internalName;
		for (const wchar_t* current = fileName; *current != L'\0'; ++current) {
			if (*current > 0x7F) {
				Error(
					"ShaderInclude",
					"DXC requested a non-canonical include name."
				);
				return E_FAIL;
			}
			internalName.push_back(static_cast<char>(*current));
		}
		std::ranges::replace(internalName, '\\', '/');
		constexpr std::string_view kInternalPrefix =
			"__unnamed_shader_include__/";
		// DXC may prefix nested includes with their parent internal directory.
		const size_t prefixPosition = internalName.rfind(kInternalPrefix);
		if (prefixPosition != std::string::npos) {
			internalName.erase(0, prefixPosition);
		}

		const auto includeIndex =
			mCompileUnit.includeIndexByInternalName.find(internalName);
		if (includeIndex == mCompileUnit.includeIndexByInternalName.end()) {
			Error(
				"ShaderInclude",
				"DXC requested an include outside the resolved compile unit: internalName='{}' root='{}' mount='{}'",
				internalName,
				mCompileUnit.rootDiagnosticPath,
				mCompileUnit.rootMountId
			);
			return E_FAIL;
		}

		const ShaderCompileIncludeEntry& entry =
			mCompileUnit.includeEntries[includeIndex->second];
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> blob;
		const HRESULT result = mDxcUtils->CreateBlob(
			entry.rewrittenSource.data(),
			static_cast<UINT32>(entry.rewrittenSource.size()),
			DXC_CP_UTF8,
			blob.ReleaseAndGetAddressOf()
		);
		if (FAILED(result)) {
			return result;
		}
		*includeSource = blob.Detach();
		return S_OK;
	}

	MountAwareDxcIncludeHandler::MountAwareDxcIncludeHandler(
		IDxcUtils& dxcUtils,
		const ShaderCompileUnit& compileUnit
	) : mDxcUtils(&dxcUtils),
	    mCompileUnit(compileUnit) {
	}
}
