#include "UiCanvasComponent.h"

#include <algorithm>

#include <imgui.h>

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/assets/types/UiDocumentAssetData.h"
#include "core/filesystem/Path.h"
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

		std::string ToModeString(const UI_CANVAS_SPACE_MODE mode) {
			switch (mode) {
				case UI_CANVAS_SPACE_MODE::SCREEN: return "Screen";
				case UI_CANVAS_SPACE_MODE::WORLD_BILLBOARD: return
						"WorldBillboard";
				case UI_CANVAS_SPACE_MODE::WORLD_PLANE: return "WorldPlane";
				default: return "Screen";
			}
		}

		UI_CANVAS_SPACE_MODE ParseMode(const std::string_view value) {
			if (value == "WorldBillboard") {
				return UI_CANVAS_SPACE_MODE::WORLD_BILLBOARD;
			}
			if (value == "WorldPlane") {
				return UI_CANVAS_SPACE_MODE::WORLD_PLANE;
			}
			return UI_CANVAS_SPACE_MODE::SCREEN;
		}

		std::string ToBillboardDepthModeString(
			const UI_CANVAS_BILLBOARD_DEPTH_MODE mode
		) {
			switch (mode) {
				case UI_CANVAS_BILLBOARD_DEPTH_MODE::DEPTH_TEST: return
						"DepthTest";
				case UI_CANVAS_BILLBOARD_DEPTH_MODE::ALWAYS_FRONT: return
						"AlwaysFront";
				default: return "DepthTest";
			}
		}

		UI_CANVAS_BILLBOARD_DEPTH_MODE ParseBillboardDepthMode(
			const std::string_view value
		) {
			if (value == "AlwaysFront") {
				return UI_CANVAS_BILLBOARD_DEPTH_MODE::ALWAYS_FRONT;
			}
			return UI_CANVAS_BILLBOARD_DEPTH_MODE::DEPTH_TEST;
		}

		Path ResolveDefaultUiAssetPath() {
			const auto* runtimeContext = ServiceLocator::Get<
				GameRuntimeContext>();
			if (!runtimeContext) {
				return {};
			}
			return runtimeContext->defaultUiDocumentPath.LexicallyNormal();
		}
	}

	UiCanvasComponent::UiCanvasComponent() :
		mUiAssetPath(ResolveDefaultUiAssetPath()) {
	}

	UiCanvasComponent::~UiCanvasComponent() = default;

	void UiCanvasComponent::Deserialize(const JsonReader& reader) {
		if (reader.Has("uiAssetPath")) {
			SetUiAssetPath(Path(reader["uiAssetPath"].GetString()));
		}
		if (reader.Has("spaceMode")) {
			SetSpaceMode(ParseMode(reader["spaceMode"].GetString()));
		}
		if (reader.Has("billboardDepthMode")) {
			SetBillboardDepthMode(
				ParseBillboardDepthMode(
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
	}

	bool UiCanvasComponent::Deserialize(
		const JsonReader& reader, const SceneDeserializeContext& context
	) {
		Deserialize(reader);
		mAssetValidationPolicy = ToAssetReferenceValidationPolicy(
			context.loadOptions.assetValidationPolicy
		);

		if (!context.assetManager) {
			Error(
				kChannel,
				"AssetManager is not available while loading UI asset '{}'.",
				mUiAssetPath
			);
			return !IsStrictAssetValidation(mAssetValidationPolicy);
		}

		const Gui::UiDeserializeContext uiContext{
			.assetManager          = *context.assetManager,
			.assetValidationPolicy = mAssetValidationPolicy,
		};
		const bool loaded = EnsureRuntimeLoaded(uiContext);
		return loaded || !IsStrictAssetValidation(mAssetValidationPolicy);
	}

	void UiCanvasComponent::Serialize(JsonWriter& writer) const {
		writer.Key("uiAssetPath");
		writer.Write(mUiAssetPath.ToGenericUtf8());

		writer.Key("spaceMode");
		writer.Write(ToModeString(mSpaceMode));

		writer.Key("billboardDepthMode");
		writer.Write(ToBillboardDepthModeString(mBillboardDepthMode));

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
		std::string uiAssetPath = mUiAssetPath.ToGenericUtf8();
		if (
			ImGuiWidgets::AssetPathPicker(
				"UI Asset Path",
				uiAssetPath,
				ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::UI_DOCUMENT)
			)
		) {
			SetUiAssetPath(Path(uiAssetPath));
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

	void UiCanvasComponent::SetUiAssetPath(Path path) {
		path = path.IsEmpty() ? Path() : path.LexicallyNormal();
		if (mUiAssetPath == path) {
			return;
		}
		mUiAssetPath = std::move(path);
		mUiAssetId   = kInvalidAssetID;
		InvalidateRuntime();
	}

	const Path& UiCanvasComponent::GetUiAssetPath() const {
		return mUiAssetPath;
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
		if (mUiAssetPath.IsEmpty()) {
			if (!mLoggedLoadFailure) {
				Error(kChannel, "UI asset path is empty.");
				mLoggedLoadFailure = true;
			}
			return false;
		}

		if (mUiAssetId == kInvalidAssetID || mLoadedAssetPath != mUiAssetPath) {
			mUiAssetId = context.assetManager.LoadFromFile(
				mUiAssetPath, ASSET_TYPE::UI_DOCUMENT
			);
			mLoadedAssetVersion = 0;
		}
		if (mUiAssetId == kInvalidAssetID) {
			if (!mLoggedLoadFailure) {
				Error(
					kChannel, "Failed to resolve UI asset '{}'.", mUiAssetPath
				);
				mLoggedLoadFailure = true;
			}
			return false;
		}

		const auto& meta = context.assetManager.Meta(mUiAssetId);
		if (
			mRuntimeRoot &&
			mLoadedAssetPath == mUiAssetPath &&
			mLoadedAssetVersion == meta.version
		) {
			return true;
		}

		const auto* assetData = context.assetManager.Get<UiDocumentAssetData>(
			mUiAssetId
		);
		if (!assetData) {
			if (!mLoggedLoadFailure) {
				Error(
					kChannel, "UI asset '{}' payload is invalid.", mUiAssetPath
				);
				mLoggedLoadFailure = true;
			}
			return false;
		}

		const auto document = Gui::UiDocument::LoadFromJson(
			JsonReader(assetData->rootJson),
			mUiAssetPath.ToGenericUtf8(),
			context
		);
		if (!document) {
			if (!mLoggedLoadFailure) {
				Error(kChannel, "Failed to load UI asset '{}'.", mUiAssetPath);
				mLoggedLoadFailure = true;
			}
			return false;
		}

		auto rootWidget = document->TakeRootWidget();
		if (!rootWidget) {
			if (!mLoggedLoadFailure) {
				Error(
					kChannel, "UI asset '{}' has no root widget.", mUiAssetPath
				);
				mLoggedLoadFailure = true;
			}
			return false;
		}

		mRuntimeRoot = std::make_unique<Gui::UiRoot>();
		mRuntimeRoot->AddChild(std::move(rootWidget));
		mLoadedAssetPath    = mUiAssetPath;
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
		mLoadedAssetPath.Clear();
		mLoadedAssetVersion = 0;
	}

	REGISTER_COMPONENT(UiCanvasComponent);
}
