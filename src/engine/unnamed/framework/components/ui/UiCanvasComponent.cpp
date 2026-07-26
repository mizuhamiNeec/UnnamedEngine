#include "UiCanvasComponent.h"

#include <algorithm>

#include <imgui.h>

#include "engine/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/assets/types/UiDocumentAssetData.h"
#include "core/content/ContentPathResolver.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/gui/UiDocument.h"
#include "engine/gui/UiDeserializeContext.h"
#include "engine/gui/UiRoot.h"
#include "engine/scene/SceneLoadOptions.h"
#include "engine/game/GameRuntimeContext.h"
#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel = "UiCanvasComponent";

		std::optional<VirtualPath> ResolveDefaultUiDocumentPath() {
			const auto* runtimeContext = ServiceLocator::Get<
				GameRuntimeContext>();
			if (!runtimeContext) {
				return std::nullopt;
			}
			return runtimeContext->defaultUiDocument;
		}
	}

	std::string_view ToString(const UI_CANVAS_SPACE_MODE mode) {
		switch (mode) {
			case UI_CANVAS_SPACE_MODE::SCREEN: return "Screen";
			case UI_CANVAS_SPACE_MODE::WORLD_BILLBOARD: return
					"WorldBillboard";
			case UI_CANVAS_SPACE_MODE::WORLD_PLANE: return "WorldPlane";
			default: return "Screen";
		}
	}

	UI_CANVAS_SPACE_MODE ParseUiCanvasSpaceMode(
		const std::string_view value
	) {
		if (value == "WorldBillboard") {
			return UI_CANVAS_SPACE_MODE::WORLD_BILLBOARD;
		}
		if (value == "WorldPlane") {
			return UI_CANVAS_SPACE_MODE::WORLD_PLANE;
		}
		return UI_CANVAS_SPACE_MODE::SCREEN;
	}

	std::string_view ToString(const UI_CANVAS_BILLBOARD_DEPTH_MODE mode) {
		switch (mode) {
			case UI_CANVAS_BILLBOARD_DEPTH_MODE::DEPTH_TEST: return
					"DepthTest";
			case UI_CANVAS_BILLBOARD_DEPTH_MODE::ALWAYS_FRONT: return
					"AlwaysFront";
			default: return "DepthTest";
		}
	}

	UI_CANVAS_BILLBOARD_DEPTH_MODE ParseUiCanvasBillboardDepthMode(
		const std::string_view value
	) {
		if (value == "AlwaysFront") {
			return UI_CANVAS_BILLBOARD_DEPTH_MODE::ALWAYS_FRONT;
		}
		return UI_CANVAS_BILLBOARD_DEPTH_MODE::DEPTH_TEST;
	}

	UiCanvasComponent::UiCanvasComponent() :
		mUiDocumentPath(ResolveDefaultUiDocumentPath()) {
	}

	UiCanvasComponent::~UiCanvasComponent() = default;

	void UiCanvasComponent::Deserialize(const JsonReader& reader) {
		constexpr SceneLoadOptions options{};
		const Path scenePath("<direct component deserialize>");
		const SceneDeserializeContext context{
			.loadOptions   = options,
			.assetManager  = GetAssetManager(),
			.scenePath     = scenePath,
			.entityName    = "<unknown>",
			.entityId      = 0,
			.componentType = GetStableName(),
		};
		(void)Deserialize(reader, context);
	}

	bool UiCanvasComponent::Deserialize(
		const JsonReader& reader, const SceneDeserializeContext& context
	) {
		ClearUiDocumentPath();
		mUiDocumentPath = ResolveDefaultUiDocumentPath();
		mAssetValidationPolicy = ToAssetReferenceValidationPolicy(
			context.loadOptions.assetValidationPolicy
		);

		if (reader.Has("spaceMode")) {
			SetSpaceMode(
				ParseUiCanvasSpaceMode(reader["spaceMode"].GetString())
			);
		}
		if (reader.Has("billboardDepthMode")) {
			SetBillboardDepthMode(
				ParseUiCanvasBillboardDepthMode(
					reader["billboardDepthMode"].GetString()
				)
			);
		}
		if (reader.Has("pixelSize")) {
			const JsonReader pixel = reader["pixelSize"].GetArray();
			if (pixel.Size() >= 2) {
				SetPixelSize(Vec2(pixel[0].GetFloat(), pixel[1].GetFloat()));
			}
		}
		if (reader.Has("worldSize")) {
			const JsonReader world = reader["worldSize"].GetArray();
			if (world.Size() >= 2) {
				SetWorldSize(Vec2(world[0].GetFloat(), world[1].GetFloat()));
			}
		}
		if (reader.Has("sortKey")) {
			SetSortKey(reader["sortKey"].GetInt());
		}
		if (reader.Has("receiveInput")) {
			SetReceiveInput(reader["receiveInput"].GetBool());
		}

		const JsonReader pathNode = reader["uiDocumentPath"];
		if (pathNode.Valid()) {
			mUiDocumentPath.reset();
			if (!pathNode.IsString()) {
				Error(
					kChannel,
					"Scene asset reference failed: classification='invalid virtual path' scene='{}' entity='{}' entityId={} component='{}' field='uiDocumentPath' reason='expected string'",
					context.scenePath,
					context.entityName,
					context.entityId,
					context.componentType
				);
				return !IsStrictAssetValidation(context.loadOptions);
			}

			const std::string pathValue = pathNode.GetString();
			if (pathValue.empty()) {
				return true;
			}
			mUiDocumentPath = VirtualPath::ParseContentReference(pathValue);
			if (!mUiDocumentPath.has_value()) {
				Error(
					kChannel,
					"Scene asset reference failed: classification='invalid virtual path' scene='{}' entity='{}' entityId={} component='{}' field='uiDocumentPath' virtualPath='{}'",
					context.scenePath,
					context.entityName,
					context.entityId,
					context.componentType,
					pathValue
				);
				return !IsStrictAssetValidation(context.loadOptions);
			}
		}

		if (!mUiDocumentPath.has_value()) {
			return true;
		}

		if (!context.assetManager) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='uiDocumentPath' virtualPath='{}' reason='AssetManager unavailable'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType,
				mUiDocumentPath->String()
			);
			ClearUiDocumentPath();
			return !IsStrictAssetValidation(mAssetValidationPolicy);
		}

		const std::optional<ResolvedContentFile> resolvedFile =
			context.assetManager->GetContentPathResolver().ResolveFile(
				*mUiDocumentPath
			);
		if (!resolvedFile.has_value()) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='unresolved content' scene='{}' entity='{}' entityId={} component='{}' field='uiDocumentPath' virtualPath='{}'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType,
				mUiDocumentPath->String()
			);
			ClearUiDocumentPath();
			return !IsStrictAssetValidation(mAssetValidationPolicy);
		}

		mUiDocumentAssetId = context.assetManager->LoadAsset(
			*mUiDocumentPath, ASSET_TYPE::UI_DOCUMENT
		);
		if (mUiDocumentAssetId == kInvalidAssetID) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='uiDocumentPath' virtualPath='{}' mount='{}' physicalPath='{}'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType,
				mUiDocumentPath->String(),
				resolvedFile->mountId,
				resolvedFile->resolvedPath
			);
			ClearUiDocumentPath();
			return !IsStrictAssetValidation(mAssetValidationPolicy);
		}

		const Gui::UiDeserializeContext uiContext{
			.assetManager          = *context.assetManager,
			.assetValidationPolicy = mAssetValidationPolicy,
		};
		const bool loaded = EnsureRuntimeLoaded(uiContext);
		if (!loaded) {
			Error(
				kChannel,
				"Scene asset reference failed: classification='asset load failure' scene='{}' entity='{}' entityId={} component='{}' field='uiDocumentPath' virtualPath='{}' mount='{}' physicalPath='{}' reason='UI document deserialize failed'",
				context.scenePath,
				context.entityName,
				context.entityId,
				context.componentType,
				mUiDocumentPath->String(),
				resolvedFile->mountId,
				resolvedFile->resolvedPath
			);
			ClearUiDocumentPath();
		}
		return loaded || !IsStrictAssetValidation(mAssetValidationPolicy);
	}

	void UiCanvasComponent::Serialize(JsonWriter& writer) const {
		if (mUiDocumentPath.has_value()) {
			writer.Key("uiDocumentPath");
			writer.Write(mUiDocumentPath->String());
		}

		writer.Key("spaceMode");
		writer.Write(std::string(ToString(mSpaceMode)));

		writer.Key("billboardDepthMode");
		writer.Write(std::string(ToString(mBillboardDepthMode)));

		writer.Key("pixelSize");
		writer.BeginArray();
		writer.Write(mPixelSize.x);
		writer.Write(mPixelSize.y);
		writer.EndArray();

		writer.Key("worldSize");
		writer.BeginArray();
		writer.Write(mWorldSize.x);
		writer.Write(mWorldSize.y);
		writer.EndArray();

		writer.Key("sortKey");
		writer.Write(mSortKey);

		writer.Key("receiveInput");
		writer.Write(mReceiveInput);
	}

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
	void UiCanvasComponent::DrawInspectorImGui() {
		std::string uiDocumentPath = mUiDocumentPath.has_value() ?
			mUiDocumentPath->String() : std::string{};
		// アセット更新時だけランタイムツリーを作り直し、通常フレームでは再利用する
		if (
			ImGuiWidgets::AssetPathPicker(
				"UI Document Path",
				uiDocumentPath,
				ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::UI_DOCUMENT)
			)
		) {
			const std::optional<VirtualPath> path =
				VirtualPath::ParseContentReference(uiDocumentPath);
			AssetManager* assetManager = GetAssetManager();
			if (!path.has_value() || !assetManager) {
				ClearUiDocumentPath();
			} else {
				(void)SetUiDocumentPath(*path, *assetManager);
			}
		}

		constexpr const char* kModes[] = {
			"Screen", "WorldBillboard", "WorldPlane"
		};
		int mode = static_cast<int>(mSpaceMode);
		if (ImGui::Combo("Space Mode", &mode, kModes, IM_ARRAYSIZE(kModes))) {
			SetSpaceMode(static_cast<UI_CANVAS_SPACE_MODE>(mode));
		}

		if (mSpaceMode == UI_CANVAS_SPACE_MODE::WORLD_BILLBOARD) {
			constexpr const char* kDepthModes[] = {"DepthTest", "AlwaysFront"};
			int depthMode = static_cast<int>(mBillboardDepthMode);
			if (ImGui::Combo(
				"Billboard Depth",
				&depthMode,
				kDepthModes,
				IM_ARRAYSIZE(kDepthModes)
			)) {
				SetBillboardDepthMode(
					static_cast<UI_CANVAS_BILLBOARD_DEPTH_MODE>(depthMode)
				);
			}
		}

		float pixel[2] = {mPixelSize.x, mPixelSize.y};
		if (ImGui::DragFloat2("Pixel Size", pixel, 1.0f, 1.0f, 8192.0f)) {
			SetPixelSize(Vec2(pixel[0], pixel[1]));
		}

		float world[2] = {mWorldSize.x, mWorldSize.y};
		if (ImGui::DragFloat2("World Size", world, 0.01f, 0.01f, 1000.0f)) {
			SetWorldSize(Vec2(world[0], world[1]));
		}

		ImGui::DragInt("Sort Key", &mSortKey, 1.0f);
		ImGui::Checkbox("Receive Input", &mReceiveInput);
	}
#endif
	uint32_t UiCanvasComponent::GetIcon() const {
		return kIconMonitor;
	}

	bool UiCanvasComponent::SetUiDocumentPath(
		const VirtualPath& path, AssetManager& assetManager
	) {
		const AssetID assetId = assetManager.LoadAsset(
			path, ASSET_TYPE::UI_DOCUMENT
		);
		if (assetId == kInvalidAssetID) {
			ClearUiDocumentPath();
			return false;
		}

		if (
			mUiDocumentPath == path &&
			mUiDocumentAssetId == assetId
		) {
			const Gui::UiDeserializeContext context{
				.assetManager          = assetManager,
				.assetValidationPolicy = mAssetValidationPolicy,
			};
			if (EnsureRuntimeLoaded(context)) {
				return true;
			}
			ClearUiDocumentPath();
			return false;
		}
		mUiDocumentPath    = path;
		mUiDocumentAssetId = assetId;
		InvalidateRuntime();
		const Gui::UiDeserializeContext context{
			.assetManager          = assetManager,
			.assetValidationPolicy = mAssetValidationPolicy,
		};
		if (EnsureRuntimeLoaded(context)) {
			return true;
		}
		ClearUiDocumentPath();
		return false;
	}

	void UiCanvasComponent::ClearUiDocumentPath() {
		mUiDocumentPath.reset();
		mUiDocumentAssetId = kInvalidAssetID;
		InvalidateRuntime();
	}

	const std::optional<VirtualPath>& UiCanvasComponent::GetUiDocumentPath(
	) const {
		return mUiDocumentPath;
	}

	AssetID UiCanvasComponent::GetUiDocumentAssetId() const {
		return mUiDocumentAssetId;
	}

	void UiCanvasComponent::SetSpaceMode(const UI_CANVAS_SPACE_MODE mode) {
		mSpaceMode = mode;
	}

	UI_CANVAS_SPACE_MODE UiCanvasComponent::GetSpaceMode() const {
		return mSpaceMode;
	}

	void UiCanvasComponent::SetBillboardDepthMode(
		const UI_CANVAS_BILLBOARD_DEPTH_MODE mode
	) {
		mBillboardDepthMode = mode;
	}

	UI_CANVAS_BILLBOARD_DEPTH_MODE UiCanvasComponent::GetBillboardDepthMode()
	const {
		return mBillboardDepthMode;
	}

	void UiCanvasComponent::SetPixelSize(const Vec2& size) {
		mPixelSize = Vec2(std::max(1.0f, size.x), std::max(1.0f, size.y));
	}

	Vec2 UiCanvasComponent::GetPixelSize() const {
		return mPixelSize;
	}

	void UiCanvasComponent::SetWorldSize(const Vec2& size) {
		mWorldSize = Vec2(std::max(0.01f, size.x), std::max(0.01f, size.y));
	}

	Vec2 UiCanvasComponent::GetWorldSize() const {
		return mWorldSize;
	}

	void UiCanvasComponent::SetSortKey(const int32_t sortKey) {
		mSortKey = sortKey;
	}

	int32_t UiCanvasComponent::GetSortKey() const {
		return mSortKey;
	}

	void UiCanvasComponent::SetReceiveInput(const bool receiveInput) {
		mReceiveInput = receiveInput;
	}

	bool UiCanvasComponent::GetReceiveInput() const {
		return mReceiveInput;
	}

	bool UiCanvasComponent::EnsureRuntimeLoaded() {
		AssetManager* assetManager = GetAssetManager();
		if (!assetManager) {
			if (!mLoggedLoadFailure) {
				Error(kChannel, "AssetManager is not available.");
				mLoggedLoadFailure = true;
			}
			return false;
		}

		const Gui::UiDeserializeContext context{
			.assetManager          = *assetManager,
			.assetValidationPolicy = mAssetValidationPolicy,
		};
		return EnsureRuntimeLoaded(context);
	}

	bool UiCanvasComponent::EnsureRuntimeLoaded(
		const Gui::UiDeserializeContext& context
	) {
		if (!mUiDocumentPath.has_value()) {
			if (!mLoggedLoadFailure) {
				Error(kChannel, "UI document path is not set.");
				mLoggedLoadFailure = true;
			}
			return false;
		}

		if (mUiDocumentAssetId == kInvalidAssetID) {
			if (!mLoggedLoadFailure) {
				Error(
					kChannel,
					"UI document '{}' has no loaded AssetID.",
					mUiDocumentPath->String()
				);
				mLoggedLoadFailure = true;
			}
			return false;
		}

		const auto& meta = context.assetManager.Meta(mUiDocumentAssetId);
		if (
			mRuntimeRoot &&
			mLoadedAssetVersion == meta.version
		) {
			return true;
		}

		const auto* assetData = context.assetManager.Get<UiDocumentAssetData>(
			mUiDocumentAssetId
		);
		if (!assetData) {
			if (!mLoggedLoadFailure) {
				Error(
					kChannel,
					"UI document '{}' payload is invalid.",
					mUiDocumentPath->String()
				);
				mLoggedLoadFailure = true;
			}
			return false;
		}

		const auto document = Gui::UiDocument::LoadFromJson(
			JsonReader(assetData->rootJson),
			mUiDocumentPath->String(),
			context
		);
		if (!document) {
			if (!mLoggedLoadFailure) {
				Error(
					kChannel,
					"Failed to load UI document '{}'.",
					mUiDocumentPath->String()
				);
				mLoggedLoadFailure = true;
			}
			return false;
		}

		// ドキュメント定義は共有し、実行時ツリーは Canvas ごとに所有する
		auto rootWidget = document->TakeRootWidget();
		if (!rootWidget) {
			if (!mLoggedLoadFailure) {
				Error(
					kChannel,
					"UI document '{}' has no root widget.",
					mUiDocumentPath->String()
				);
				mLoggedLoadFailure = true;
			}
			return false;
		}

		auto runtimeRoot = std::make_unique<Gui::UiRoot>();
		runtimeRoot->AddChild(std::move(rootWidget));
		mRuntimeRoot        = std::move(runtimeRoot);
		mLoadedAssetVersion = meta.version;
		mLoggedLoadFailure  = false;
		return true;
	}

	void UiCanvasComponent::TickRuntime(const float deltaTime) const {
		if (mRuntimeRoot) {
			mRuntimeRoot->Tick(deltaTime);
		}
	}

	Gui::UiRoot* UiCanvasComponent::GetRuntimeRoot() const {
		return mRuntimeRoot.get();
	}

	void UiCanvasComponent::OnDetached() {
		InvalidateRuntime();
	}

	void UiCanvasComponent::InvalidateRuntime() {
		mRuntimeRoot.reset();
		mLoadedAssetVersion = 0;
		mLoggedLoadFailure  = false;
	}

	REGISTER_COMPONENT(UiCanvasComponent);
}
