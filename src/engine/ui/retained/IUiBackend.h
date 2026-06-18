#pragma once
#include "UiTypes.h"

#include "core/assets/AssetID.h"

#include "core/math/Vec2.h"
#include "core/math/Vec4.h"

namespace Unnamed::UI::Retained {
	struct UiBackendFrameContext {
		UiRect viewportRect = {};
	};

	struct UiRectPaintData {
		UiRect rect     = {};
		UiRect clipRect = {};

		Vec4 color = {};

		float cornerRadius = 0.0f;
	};

	struct UiImagePaintData {
		UiRect rect     = {};
		UiRect clipRect = {};

		AssetID textureId = {};

		Vec2 uvMin = Vec2::zero;
		Vec2 uvMax = Vec2::one;

		Vec4 tintColor = Vec4::white;
	};

	class IUiBackend {
	public:
		virtual ~IUiBackend() = default;

		virtual void BeginFrame(const UiBackendFrameContext& frameContext) = 0;

		virtual void EndFrame() = 0;

		[[nodiscard]] virtual UiInteractionResult ProcessInteraction(
			UiNodeId      nodeId,
			const UiRect& interactionRect,
			const UiRect& clipRect,
			bool          enabled
		) = 0;

		virtual void DrawRect(const UiRectPaintData& paintData) = 0;
		virtual void DrawImage(const UiImagePaintData& paintData) = 0;
	};
}
