#include "pch.h"
#include "UiSystem.h"

namespace Unnamed::UI::Retained {
	constexpr float kMaxUiDeltaTime = 0.1f;

	namespace {
		UiRect CalculateWorldRect(
			const UiRect& localRect, const UiSystem::TraversalContext& context
		) {
			return {
				.min = context.origin + localRect.min * context.size,
				.max = context.origin + localRect.max * context.size
			};
		}

		UiRect ScaleRectAroundCenter(
			const UiRect& rect, const Vec2& scale
		) {
			const Vec2 center   = rect.Center();
			const Vec2 halfSize = rect.Size() * 0.5f * scale;

			return {
				.min = center - halfSize,
				.max = center + halfSize
			};
		}

		const UiButtonVisualStyle& GetButtonVisualStyle(
			const UiButtonStyle& style, const UI_INTERACTION_STATE state
		) {
			switch (state) {
				case UI_INTERACTION_STATE::NORMAL: return style.normal;
				case UI_INTERACTION_STATE::HOVERED: return style.hovered;
				case UI_INTERACTION_STATE::PRESSED: return style.pressed;
				case UI_INTERACTION_STATE::DISABLED: return style.disabled;
			}

			return style.normal;
		}
	}

	void UiSystem::Render(
		const UiDocument&     document, const UiNodeHandle rootNode,
		IUiBackend&           backend,
		const UiFrameContext& frameContext
	) {
		// GetEvents()のイベントは次回のRender()まで有効
		mEvents.clear();

		UiBackendFrameContext backendFrameContext = {};
		backendFrameContext.viewportRect          = frameContext.viewportRect;

		backend.BeginFrame(backendFrameContext);

		if (!rootNode.IsValid()) {
			backend.EndFrame();
			return;
		}

		float deltaTime = std::max(frameContext.unscaledDeltaTime, 0.0f);

		// スパイクで移動しすぎないようにクランプ
		deltaTime = std::min(deltaTime, kMaxUiDeltaTime);

		TraversalContext traversalContext = {};
		traversalContext.origin           = frameContext.viewportRect.min;

		traversalContext.size = Vec2::one;

		traversalContext.contentTintColor = Vec4::white;

		traversalContext.opacity = 1.0f;

		traversalContext.clipRect = frameContext.viewportRect;

		ProcessNode(document, rootNode, backend, traversalContext, deltaTime);

		backend.EndFrame();
	}

	std::span<const UiEvent> UiSystem::GetEvents() const {
		return std::span(mEvents.data(), mEvents.size());
	}

	void UiSystem::ProcessNode(
		const UiDocument&       document, const UiNodeHandle nodeHandle,
		IUiBackend&             backend,
		const TraversalContext& context, const float deltaTime
	) {
		UiNode node = document.GetNode(nodeHandle);

		if (!node.visible) {
			return;
		}

		switch (node.type) {
			case UI_NODE_TYPE::BUTTON: {
				UiButtonData* button = std::get_if<UiButtonData>(&node.data);

				UASSERT(button != nullptr);

				if (button == nullptr) {
					return;
				}

				ProcessButton(
					document,
					nodeHandle,
					node,
					*button,
					backend,
					context,
					deltaTime
				);
				break;
			}
			case UI_NODE_TYPE::IMAGE: {
				const UiImageData* image = std::get_if<UiImageData>(&node.data);

				UASSERT(image != nullptr);

				if (image == nullptr) {
					return;
				}

				ProcessImage(
					node,
					*image,
					backend,
					context
				);
			}
		}
	}

	void UiSystem::ProcessButton(
		const UiDocument&       document, const UiNodeHandle nodeHandle,
		const UiNode&           node,
		UiButtonData&           button, IUiBackend&  backend,
		const TraversalContext& context, const float deltaTime
	) {
		(void)nodeHandle;

		UiButtonRuntimeState& runtime = button.runtimeState;

		if (!runtime.initialized) {
			const UiButtonVisualStyle& normal = button.style.normal;
			runtime.backgroundColor.SetImmediate(normal.contentTintColor);
			runtime.contentTintColor.SetImmediate(normal.contentTintColor);
			runtime.size.SetImmediate(normal.size);
			runtime.opacity.SetImmediate(normal.opacity);

			runtime.interactionState =
				button.enabled ?
					UI_INTERACTION_STATE::NORMAL :
					UI_INTERACTION_STATE::DISABLED;

			runtime.initialized = true;
		}

		const UiRect layoutRect = CalculateWorldRect(node.localRect, context);

		const UiInteractionResult interaction =
			backend.ProcessInteraction(
				node.id,
				layoutRect,
				context.clipRect,
				button.enabled
			);

		auto nextState = UI_INTERACTION_STATE::NORMAL;

		if (!button.enabled) {
			nextState = UI_INTERACTION_STATE::DISABLED;
		} else if (interaction.held) {
			nextState = UI_INTERACTION_STATE::PRESSED;
		} else if (interaction.hovered) {
			nextState = UI_INTERACTION_STATE::HOVERED;
		}

		if (nextState != runtime.interactionState) {
			runtime.interactionState = nextState;

			const UiButtonVisualStyle& target =
				GetButtonVisualStyle(button.style, nextState);

			runtime.backgroundColor.AnimateTo(
				target.backgroundColor, target.transitionDuration
			);
			runtime.contentTintColor.AnimateTo(
				target.contentTintColor, target.transitionDuration
			);
			runtime.size.AnimateTo(
				target.size, target.transitionDuration
			);
			runtime.opacity.AnimateTo(
				target.opacity, target.transitionDuration
			);
		}

		runtime.backgroundColor.Update(deltaTime);
		runtime.contentTintColor.Update(deltaTime);
		runtime.size.Update(deltaTime);
		runtime.opacity.Update(deltaTime);

		const UiRect visualRect =
			ScaleRectAroundCenter(layoutRect, runtime.size.Value());

		Vec4 backgroundColor = runtime.backgroundColor.Value() * context.
		                       contentTintColor;

		// 親から透明度を引き継ぐ
		backgroundColor.w *= context.opacity * runtime.opacity.Value();

		backend.DrawRect(
			UiRectPaintData{
				.rect         = visualRect,
				.clipRect     = context.clipRect,
				.color        = backgroundColor,
				.cornerRadius = 4.0f
			}
		);

		if (interaction.clicked) {
			mEvents.emplace_back(
				UiEvent{
					.target = node.id,
					.type   = UI_EVENT_TYPE::CLICK
				}
			);
		}

		TraversalContext childContext;
		childContext.origin = visualRect.min;
		childContext.size   = context.size * runtime.size.Value();

		childContext.contentTintColor =
			context.contentTintColor * runtime.contentTintColor.Value();

		childContext.opacity = context.opacity * runtime.opacity.Value();

		childContext.clipRect = context.clipRect;

		ProcessChildren(
			document,
			node,
			backend,
			childContext,
			deltaTime
		);
	}

	void UiSystem::ProcessImage(
		const UiNode& node, const UiImageData& image, IUiBackend& backend,
		const TraversalContext& context
	) {
		Vec4 tintColor =
			image.tintColor * context.contentTintColor;

		tintColor.w *= context.opacity;

		backend.DrawImage(
			UiImagePaintData{
				.rect = CalculateWorldRect(
					node.localRect,
					context
				),
				.clipRect  = context.clipRect,
				.textureId = image.textureId,
				.uvMin     = image.uvMin,
				.uvMax     = image.uvMax,
				.tintColor = tintColor
			}
		);
	}

	void UiSystem::ProcessChildren(
		const UiDocument&       document, const UiNode& parent,
		IUiBackend&             backend,
		const TraversalContext& context, const float deltaTime
	) {
		UiNodeHandle childHandle = parent.firstChild;

		while (childHandle.IsValid()) {
			const UiNode childNode = document.GetNode(childHandle);

			// ProcessNode内で遅延削除をする場合に備え、次のハンドルを取っておく
			const UiNodeHandle nextSibling = childNode.nextSibling;

			ProcessNode(
				document,
				childHandle,
				backend,
				context,
				deltaTime
			);

			childHandle = nextSibling;
		}
	}
}
