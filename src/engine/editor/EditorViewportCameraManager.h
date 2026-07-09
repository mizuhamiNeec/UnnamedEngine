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

	struct ViewportCameraBinding {
		ViewportCameraBindingKind kind = ViewportCameraBindingKind::
			EditorPerspective;
		uint64_t cameraEntityGuid = 0;
	};

	class EditorViewportCameraManager final {
	public:
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

		ResolvedCamera ResolveViewCamera(
			EditorWorld&                       editorWorld,
			std::string_view                   viewKey,
			const Render::SceneViewRenderMode& sceneViewMode,
			const ViewportCameraBinding&       binding,
			const Render::RenderCameraInput*   fallbackCamera
		);

		void SyncGameplayCameraAspect(
			EditorWorld&                       editorWorld,
			const Render::SceneViewRenderMode& sceneViewMode,
			const ViewportCameraBinding&       binding
		);

	private:
		std::unordered_map<std::string, ViewportCameraBinding> mPaneBindings;
	};
}

#endif
