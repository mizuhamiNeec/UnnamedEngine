#pragma once

#include <memory>
#include <string>

#include "core/assets/AssetID.h"
#include "core/filesystem/Path.h"

#include "engine/unnamed/framework/components/base/BaseComponent.h"

#include "core/math/Vec2.h"

namespace Unnamed::Gui {
	class UiRoot;
}

namespace Unnamed {
	enum class UI_CANVAS_SPACE_MODE : uint8_t {
		SCREEN          = 0,
		WORLD_BILLBOARD = 1,
		WORLD_PLANE     = 2,
	};

	enum class UI_CANVAS_BILLBOARD_DEPTH_MODE : uint8_t {
		DEPTH_TEST   = 0,
		ALWAYS_FRONT = 1,
	};

	class UiCanvasComponent final : public BaseComponent {
	public:
		UiCanvasComponent();
		~UiCanvasComponent() override;

		[[nodiscard]] std::string_view GetStableName() const override {
			return "engine.UiCanvas";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "UiCanvas";
		}

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		void DrawInspectorImGui() override;
#endif
		
		[[nodiscard]] uint32_t GetIcon() const override;

		void SetUiAssetPath(Path path);
		[[nodiscard]] const Path& GetUiAssetPath() const;

		void SetSpaceMode(UI_CANVAS_SPACE_MODE mode);
		[[nodiscard]] UI_CANVAS_SPACE_MODE GetSpaceMode() const;

		void SetBillboardDepthMode(UI_CANVAS_BILLBOARD_DEPTH_MODE mode);
		[[nodiscard]] UI_CANVAS_BILLBOARD_DEPTH_MODE GetBillboardDepthMode()
		const;

		void SetPixelSize(const Vec2& size);
		[[nodiscard]] Vec2 GetPixelSize() const;

		void SetWorldSize(const Vec2& size);
		[[nodiscard]] Vec2 GetWorldSize() const;

		void SetSortKey(int32_t sortKey);
		[[nodiscard]] int32_t GetSortKey() const;

		void SetReceiveInput(bool receiveInput);
		[[nodiscard]] bool GetReceiveInput() const;

		bool EnsureRuntimeLoaded();
		void TickRuntime(float deltaTime) const;
		[[nodiscard]] Gui::UiRoot* GetRuntimeRoot() const;

		void OnDetached() override;

	private:
		void                     InvalidateRuntime();
		
		Path                           mUiAssetPath;
		UI_CANVAS_SPACE_MODE           mSpaceMode          = UI_CANVAS_SPACE_MODE::SCREEN;
		UI_CANVAS_BILLBOARD_DEPTH_MODE mBillboardDepthMode =
			UI_CANVAS_BILLBOARD_DEPTH_MODE::DEPTH_TEST;
		Vec2                 mPixelSize = Vec2(1920.0f, 1080.0f);
		Vec2                 mWorldSize = Vec2(2.0f, 1.125f);
		int32_t              mSortKey = 0;
		bool                 mReceiveInput = true;

		Path                           mLoadedAssetPath;
		AssetID                        mUiAssetId = kInvalidAssetID;
		uint64_t                       mLoadedAssetVersion = 0;
		std::unique_ptr<Gui::UiRoot>   mRuntimeRoot;
		bool                           mLoggedLoadFailure = false;
	};
}
