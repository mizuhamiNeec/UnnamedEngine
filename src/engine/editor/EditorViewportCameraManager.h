#pragma once
#ifdef _DEBUG

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "engine/render/frame/RenderFrameInputs.h"

namespace Unnamed {
	class EditorWorld;

	enum class ViewportCameraBindingKind : uint8_t {
		EditorPerspective = 0,
		ActiveGameCamera  = 1,
		CameraEntity      = 2,
	};

	/// @brief ViewportCameraBindingは、ViewportCameraの論理識別子とruntime resource参照の対応を保持します
	struct ViewportCameraBinding {
		ViewportCameraBindingKind kind = ViewportCameraBindingKind::
			EditorPerspective;
		uint64_t cameraEntityGuid = 0;
	};

	/// @brief EditorViewportCameraManagerは、viewportごとのcamera bindingとactive camera解決を管理します
	class EditorViewportCameraManager final {
	public:
		/// @brief ResolvedCameraは、viewportが使用するcamera componentとtransformの非所有参照を保持します
		struct ResolvedCamera {
			Render::RenderCameraInput input          = {};
			bool                      isOrthographic = false;
		};

		void SetPaneBinding(
			std::string_view             viewKey,
			const ViewportCameraBinding& binding
		);
		[[nodiscard]] ViewportCameraBinding GetPaneBinding(
			std::string_view viewKey
		) const;

		static ResolvedCamera ResolveViewCamera(
			EditorWorld&                       editorWorld,
			std::string_view                   viewKey,
			const Render::SceneViewRenderMode& sceneViewMode,
			const ViewportCameraBinding&       binding,
			const Render::RenderCameraInput*   fallbackCamera
		);

		static void SyncGameplayCameraAspect(
			EditorWorld&                       editorWorld,
			const Render::SceneViewRenderMode& sceneViewMode,
			const ViewportCameraBinding&       binding
		);

	private:
		std::unordered_map<std::string, ViewportCameraBinding> mPaneBindings;
	};
}

#endif
