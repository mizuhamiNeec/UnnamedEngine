#include <pch.h>

#include <chrono>
#include <fstream>

#include "core/assets/AssetManager.h"
#include "core/assets/loader/EditorGuiLoader.h"
#include "core/content/ContentPathResolver.h"
#include "core/filesystem/Path.h"
#include "core/filesystem/VirtualPath.h"
#include "engine/content/ContentMountDefinitions.h"

int main() {
	using namespace Unnamed;

	const auto uniqueSuffix = std::chrono::steady_clock::now().
		time_since_epoch().count();
	const std::filesystem::path testRootNative =
		std::filesystem::temp_directory_path() /
		std::format("UnnamedAssetPathContracts_{}", uniqueSuffix);
	const Path testRoot = Path::FromNative(testRootNative);
	const Path gameRoot = testRoot / Path("Game");
	const Path coreRoot = testRoot / Path("Core");
	const Path relativeAssetPath("fixtures/sample.contract.edgui.lua");
	const Path gameAssetPath = gameRoot / relativeAssetPath;
	const Path coreAssetPath = coreRoot / relativeAssetPath;
	const Path externalAssetPath =
		testRoot / Path("External/sample.contract.edgui.lua");

	std::error_code ec;
	std::filesystem::create_directories(gameAssetPath.ParentPath().Native(), ec);
	if (ec) {
		return EXIT_FAILURE;
	}
	std::filesystem::create_directories(coreAssetPath.ParentPath().Native(), ec);
	if (ec) {
		std::filesystem::remove_all(testRoot.Native(), ec);
		return EXIT_FAILURE;
	}
	std::filesystem::create_directories(
		externalAssetPath.ParentPath().Native(), ec
	);
	if (ec) {
		std::filesystem::remove_all(testRoot.Native(), ec);
		return EXIT_FAILURE;
	}
	{
		std::ofstream gameFile(gameAssetPath.Native(), std::ios::binary);
		std::ofstream coreFile(coreAssetPath.Native(), std::ios::binary);
		std::ofstream externalFile(externalAssetPath.Native(), std::ios::binary);
		gameFile << "function Draw() end -- Game";
		coreFile << "function Draw() end -- Core";
		externalFile << "function Draw() end -- External";
	}

	ContentPathResolver resolver;
	const bool coreMounted = resolver.MountDirectory(
		std::string(ContentMountId::kCore),
		coreRoot,
		ContentMountPriority::kCore
	);
	const bool gameMounted = resolver.MountDirectory(
		std::string(ContentMountId::kGame),
		gameRoot,
		ContentMountPriority::kGame
	);
	AssetManager assetManager(resolver);
	assetManager.RegisterLoader(std::make_unique<EditorGuiLoader>());

	bool passed = coreMounted && gameMounted;
	const auto require = [&passed](const bool condition, const char* message) {
		if (!condition) {
			std::cerr << "FAILED: " << message << '\n';
			passed = false;
		}
	};
	require(
		!VirtualPath::ParseContentReference("./fixtures/test.asset").has_value(),
		"content references must reject current-relative paths"
	);
	require(
		!VirtualPath::ParseContentReference("../fixtures/test.asset").has_value(),
		"content references must reject parent-relative paths"
	);
	require(
		!VirtualPath::ParseContentReference("content/fixtures/test.asset").has_value(),
		"content references must reject the physical content prefix"
	);
	require(
		!VirtualPath::ParseContentReference("C:/fixtures/test.asset").has_value(),
		"content references must reject absolute paths"
	);

	require(
		assetManager.LoadAssetFromFile(
			relativeAssetPath, ASSET_TYPE::EDITOR_GUI
		) == kInvalidAssetID,
		"LoadAssetFromFile must reject relative physical paths"
	);

	const VirtualPath virtualPath = VirtualPath::ParseOrThrow(
		"fixtures/sample.contract.edgui.lua"
	);
	const AssetID gameAssetId = assetManager.LoadAsset(
		virtualPath, ASSET_TYPE::EDITOR_GUI
	);
	require(gameAssetId != kInvalidAssetID, "LoadAsset must resolve Game");
	if (gameAssetId != kInvalidAssetID) {
		const AssetMetaData& meta = assetManager.Meta(gameAssetId);
		require(meta.sourcePath == gameAssetPath, "Game path must win");
		require(
			meta.sourceMountId == ContentMountId::kGame,
			"Game mount provenance must be retained"
		);
		require(
			meta.sourceVirtualPath == virtualPath,
			"VirtualPath provenance must be retained"
		);
	}

	const AssetID coreAssetId = assetManager.LoadAssetFromMount(
		virtualPath, ContentMountId::kCore, ASSET_TYPE::EDITOR_GUI
	);
	require(coreAssetId != kInvalidAssetID, "Core fixed load must succeed");
	if (coreAssetId != kInvalidAssetID) {
		const AssetMetaData& meta = assetManager.Meta(coreAssetId);
		require(meta.sourcePath == coreAssetPath, "Core fixed path must win");
		require(
			meta.sourceMountId == ContentMountId::kCore,
			"Core mount provenance must be retained"
		);
	}

	const AssetID physicalGameAssetId = assetManager.LoadAssetFromFile(
		gameAssetPath, ASSET_TYPE::EDITOR_GUI
	);
	require(
		physicalGameAssetId == gameAssetId,
		"VirtualPath and physical APIs must converge to one AssetID"
	);

	const AssetID externalAssetId = assetManager.LoadAssetFromFile(
		externalAssetPath, ASSET_TYPE::EDITOR_GUI
	);
	require(
		externalAssetId != kInvalidAssetID,
		"absolute external physical assets must remain loadable"
	);
	if (externalAssetId != kInvalidAssetID) {
		const AssetMetaData& meta = assetManager.Meta(externalAssetId);
		require(
			meta.sourcePath == externalAssetPath,
			"external path must remain physical"
		);
		require(
			meta.sourceMountId.empty(),
			"external assets must not gain a mount"
		);
		require(
			!meta.sourceVirtualPath.has_value(),
			"external assets must not gain a VirtualPath"
		);
	}

	std::filesystem::remove_all(testRoot.Native(), ec);
	if (!passed) {
		return EXIT_FAILURE;
	}
	std::cout << "Asset path contract validation passed.\n";
	return EXIT_SUCCESS;
}
