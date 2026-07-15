#include "RenderFrameInputs.h"

#include <algorithm>

namespace Unnamed::Render {
	float ResolveSceneViewAspectRatio(const SceneViewRenderMode& request) {
		switch (request.mode) {
			case SCENE_RENDER_MODE::FIXED_ASPECT_16X9: return 16.0f / 9.0f;
			case SCENE_RENDER_MODE::FIXED_ASPECT_4X3: return 4.0f / 3.0f;
			case SCENE_RENDER_MODE::HD_720P: return 1280.0f / 720.0f;
			case SCENE_RENDER_MODE::FHD_1080P: return 1920.0f / 1080.0f;
			case SCENE_RENDER_MODE::UHD_4K: return 3840.0f / 2160.0f;
			case SCENE_RENDER_MODE::FIT_VIEWPORT:
			default: {
				const float width = static_cast<float>(
					std::max(1u, request.viewportPanelWidth)
				);
				const float height = static_cast<float>(
					std::max(1u, request.viewportPanelHeight)
				);
				return width / height;
			}
		}
	}

	std::pair<uint32_t, uint32_t> ResolveSceneViewRenderExtent(
		const uint32_t             fallbackWidth,
		const uint32_t             fallbackHeight,
		const SceneViewRenderMode& request
	) {
		uint32_t width  = fallbackWidth;
		uint32_t height = fallbackHeight;

		const uint32_t panelWidth = request.viewportPanelWidth != 0 ?
			                            request.viewportPanelWidth :
			                            std::max(1u, fallbackWidth);
		const uint32_t panelHeight = request.viewportPanelHeight != 0 ?
			                             request.viewportPanelHeight :
			                             std::max(1u, fallbackHeight);

		switch (request.mode) {
			case SCENE_RENDER_MODE::FIT_VIEWPORT: {
				width  = panelWidth;
				height = panelHeight;
				break;
			}
			case SCENE_RENDER_MODE::FIXED_ASPECT_16X9: {
				width  = panelWidth;
				height = panelHeight;
				if (width * 9 > height * 16) {
					width = height * 16 / 9;
				} else {
					height = width * 9 / 16;
				}
				break;
			}
			case SCENE_RENDER_MODE::FIXED_ASPECT_4X3: {
				width  = panelWidth;
				height = panelHeight;
				if (width * 3 > height * 4) {
					width = height * 4 / 3;
				} else {
					height = width * 3 / 4;
				}
				break;
			}
			case SCENE_RENDER_MODE::HD_720P: {
				width  = 1280;
				height = 720;
				break;
			}
			case SCENE_RENDER_MODE::FHD_1080P: {
				width  = 1920;
				height = 1080;
				break;
			}
			case SCENE_RENDER_MODE::UHD_4K: {
				width  = 3840;
				height = 2160;
				break;
			}
			default: break;
		}

		width  = std::clamp(width, 2u, 8192u);
		height = std::clamp(height, 2u, 8192u);
		if ((width & 1u) != 0u) {
			--width;
		}
		if ((height & 1u) != 0u) {
			--height;
		}

		width  = std::max(2u, width);
		height = std::max(2u, height);
		return {width, height};
	}
}
