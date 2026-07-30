#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "core/assets/AssetID.h"
#include "core/filesystem/VirtualPath.h"

#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include "engine/content/AssetReferenceValidationPolicy.h"
#include "engine/gui/UiDeserializeContext.h"

#include "core/math/Vec2.h"

namespace Unnamed::Gui {
	class UiRoot;
}

namespace Unnamed {
	class AssetManager;

	enum class UI_CANVAS_SPACE_MODE : uint8_t {
		SCREEN          = 0,
		WORLD_BILLBOARD = 1,
		WORLD_PLANE     = 2,
	};

	enum class UI_CANVAS_BILLBOARD_DEPTH_MODE : uint8_t {
		DEPTH_TEST   = 0,
		ALWAYS_FRONT = 1,
	};

	/// @brief UI Canvasの空間モードを保存用文字列に変換します。
	/// @param mode 変換する空間モード。
	/// @return 保存用の文字列表現。
	[[nodiscard]] std::string_view ToString(UI_CANVAS_SPACE_MODE mode);

	/// @brief 保存用文字列からUI Canvasの空間モードへ変換します。
	/// @param value 読み込む文字列。
	/// @return 対応する空間モード。未対応値はSCREEN。
	[[nodiscard]] UI_CANVAS_SPACE_MODE ParseUiCanvasSpaceMode(
		std::string_view value
	);

	/// @brief UI Canvasのビルボード深度モードを保存用文字列に変換します。
	/// @param mode 変換するビルボード深度モード。
	/// @return 保存用の文字列表現。
	[[nodiscard]] std::string_view ToString(
		UI_CANVAS_BILLBOARD_DEPTH_MODE mode
	);

	/// @brief 保存用文字列からUI Canvasのビルボード深度モードへ変換します。
	/// @param value 読み込む文字列。
	/// @return 対応する深度モード。未対応値はDEPTH_TEST。
	[[nodiscard]] UI_CANVAS_BILLBOARD_DEPTH_MODE
	ParseUiCanvasBillboardDepthMode(std::string_view value);

	/// @brief UiCanvasComponentは、entity上のGUI canvasをWorldの入力・描画更新へ登録します
	class UiCanvasComponent final : public BaseComponent {
	public:
		UiCanvasComponent();
		~UiCanvasComponent() override = default;

		[[nodiscard]] std::string_view GetStableName() const override {
			return "engine.UiCanvas";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "UiCanvas";
		}

		void               Deserialize(const JsonReader& reader) override;
		[[nodiscard]] bool Deserialize(
			const JsonReader& reader, const SceneDeserializeContext& context
		) override;
		void Serialize(JsonWriter& writer) const override;

#if defined(_DEBUG) && defined(UNNAMED_WITH_EDITOR)
		void DrawInspectorImGui() override;
#endif

		[[nodiscard]] uint32_t GetIcon() const override;

		/// @brief UI Document参照を設定し、ロード済みAssetIDを更新します。
		[[nodiscard]] bool SetUiDocumentPath(
			const VirtualPath& path, AssetManager& assetManager
		);
		/// @brief UI Document参照とロード済み状態をクリアします。
		void                                            ClearUiDocumentPath();
		/// @brief UI Documentの論理参照を取得します。
		[[nodiscard]] const std::optional<VirtualPath>& GetUiDocumentPath(
		) const;
		/// @brief ロード済みUI DocumentのAssetIDを取得します。
		[[nodiscard]] AssetID GetUiDocumentAssetId() const;

		void SetSpaceMode(UI_CANVAS_SPACE_MODE mode);
		[[nodiscard]] UI_CANVAS_SPACE_MODE GetSpaceMode() const;

		void SetBillboardDepthMode(UI_CANVAS_BILLBOARD_DEPTH_MODE mode);
		[[nodiscard]] UI_CANVAS_BILLBOARD_DEPTH_MODE GetBillboardDepthMode()
		const;

		void               SetPixelSize(const Vec2& size);
		[[nodiscard]] Vec2 GetPixelSize() const;

		void               SetWorldSize(const Vec2& size);
		[[nodiscard]] Vec2 GetWorldSize() const;

		void                  SetSortKey(int32_t sortKey);
		[[nodiscard]] int32_t GetSortKey() const;

		void               SetReceiveInput(bool receiveInput);
		[[nodiscard]] bool GetReceiveInput() const;

		bool                       EnsureRuntimeLoaded();
		void                       TickRuntime(float deltaTime) const;
		[[nodiscard]] Gui::UiRoot* GetRuntimeRoot() const;

		void OnDetached() override;

	private:
		[[nodiscard]] bool EnsureRuntimeLoaded(
			const Gui::UiDeserializeContext& context
		);
		void InvalidateRuntime();

		std::optional<VirtualPath> mUiDocumentPath;
		UI_CANVAS_SPACE_MODE mSpaceMode = UI_CANVAS_SPACE_MODE::SCREEN;
		UI_CANVAS_BILLBOARD_DEPTH_MODE mBillboardDepthMode =
			UI_CANVAS_BILLBOARD_DEPTH_MODE::DEPTH_TEST;
		Vec2    mPixelSize    = Vec2(1920.0f, 1080.0f);
		Vec2    mWorldSize    = Vec2(2.0f, 1.125f);
		int32_t mSortKey      = 0;
		bool    mReceiveInput = true;

		AssetID                        mUiDocumentAssetId  = kInvalidAssetID;
		uint64_t                       mLoadedAssetVersion = 0;
		std::unique_ptr<Gui::UiRoot>   mRuntimeRoot;
		bool                           mLoggedLoadFailure     = false;
		AssetReferenceValidationPolicy mAssetValidationPolicy =
			AssetReferenceValidationPolicy::Permissive;
	};
}
