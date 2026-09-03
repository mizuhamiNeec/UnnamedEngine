#pragma once
#ifdef _DEBUG

#include <cstdint>
#include <functional>
#include <string>

#include <core/filesystem/Path.h>

#include "core/assets/AssetType.h"

namespace Unnamed::EditorContentBrowser {
	using AssetTypeMask                                    = uint32_t;
	inline constexpr AssetTypeMask kAssetTypeMaskAny       = 0xFFFFFFFFu;
	inline constexpr const char*   kAssetDragDropPayloadId =
		"UNNAMED_ASSET_REF";

	/// @brief AssetDragDropPayloadは、エディターイベントで送受信する値を一単位として保持します
	struct AssetDragDropPayload {
		uint16_t assetType = 0;
		char     path[512] = {};
	};

	/// @brief BrowserViewStateは、Content Browserの現在directory、選択、表示modeを保持します
	struct BrowserViewState {
		// root/current are physical paths used exclusively for enumeration.  Values
		// exported from the browser are always content virtual paths.
		Path        rootPath;
		Path        currentPath;
		Path        selectedPath;
		std::string selectedMountId;
		bool        iconView = false;
		float       iconSize = 96.0f;
	};

	using AssetOpenCallback = std::function<void(
		const std::string& path,
		ASSET_TYPE         type
	)>;

	[[nodiscard]] AssetTypeMask AssetTypeToMask(ASSET_TYPE type);
	[[nodiscard]] bool          IsAssetTypeAccepted(
		ASSET_TYPE    type,
		AssetTypeMask acceptedMask
	);

	bool DrawAssetPathPicker(
		const char*   label,
		std::string&  path,
		AssetTypeMask acceptedMask,
		const char*   helpText = nullptr
	);

	void DrawWindow(BrowserViewState& state, const char* windowName);

	void DrawWindow(
		BrowserViewState&        state,
		const char*              windowName,
		const AssetOpenCallback& onAssetOpen
	);
}

#endif
