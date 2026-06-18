#pragma once
#include <span>

#include "IUiBackend.h"
#include "UiDocument.h"
#include "UiTypes.h"

namespace Unnamed::UI::Retained {
	struct UiFrameContext {
		UiRect viewportRect      = {};
		float  unscaledDeltaTime = 0.0f;
	};

	class UiSystem {
	public:
		void Render(
			const UiDocument&     document,
			UiNodeHandle          rootNode,
			IUiBackend&           backend,
			const UiFrameContext& frameContext
		);

		[[nodiscard]] std::span<const UiEvent> GetEvents() const;

		struct TraversalContext {
			Vec2 origin = Vec2::zero;
			Vec2 size   = Vec2::one;

			Vec4 contentTintColor = Vec4::white;

			float opacity = 1.0f;

			UiRect clipRect = {};
		};

	private:
		void ProcessNode(
			const UiDocument&       document,
			UiNodeHandle            nodeHandle,
			IUiBackend&             backend,
			const TraversalContext& context,
			float                   deltaTime
		);

		void ProcessButton(
			const UiDocument&       document,
			UiNodeHandle            nodeHandle,
			const UiNode&           node,
			UiButtonData&           button,
			IUiBackend&             backend,
			const TraversalContext& context,
			float                   deltaTime
		);

		static void ProcessImage(
			const UiNode&      node,
			const UiImageData& image,
			IUiBackend&        backend, const TraversalContext& context
		);

		void ProcessChildren(
			const UiDocument&       document,
			const UiNode&           parent,
			IUiBackend&             backend,
			const TraversalContext& context,
			float                   deltaTime
		);

	private:
		std::vector<UiEvent> mEvents;
	};
}
