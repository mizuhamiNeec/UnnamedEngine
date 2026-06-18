#pragma once
#include <variant>

#include "UiAnimatedValue.h"
#include "UiTypes.h"

#include "core/assets/AssetID.h"

namespace Unnamed::UI::Retained {
	struct UiButtonVisualStyle {
		Vec4 backgroundColor  = Vec4::zero;
		Vec4 contentTintColor = Vec4::white;

		Vec2 size = Vec2::one;

		float opacity            = 1.0f;
		float transitionDuration = 0.1f;

		UiEasing easing = UiEasing::LINEAR;
	};

	struct UiButtonStyle {
		UiButtonVisualStyle normal;
		UiButtonVisualStyle hovered;
		UiButtonVisualStyle pressed;
		UiButtonVisualStyle disabled;
	};

	struct UiButtonRuntimeState {
		UiAnimatedValue<Vec4>  backgroundColor;
		UiAnimatedValue<Vec4>  contentTintColor;
		UiAnimatedValue<Vec2>  size;
		UiAnimatedValue<float> opacity;

		UI_INTERACTION_STATE interactionState = UI_INTERACTION_STATE::NORMAL;

		bool initialized = false;
	};

	struct UiButtonData {
		UiButtonStyle        style;
		UiButtonRuntimeState runtimeState;

		bool enabled = true;
	};

	struct UiImageData {
		AssetID textureId = {};

		Vec2 uvMin = Vec2::zero;
		Vec2 uvMax = Vec2::one;

		Vec4 tintColor = Vec4::white;
	};

	using UiNodeData = std::variant<UiButtonData, UiImageData>;

	struct UiNode {
		UiNodeId     id   = {};
		UI_NODE_TYPE type = UI_NODE_TYPE::IMAGE;

		UiNodeHandle parent      = {};
		UiNodeHandle firstChild  = {};
		UiNodeHandle lastChild   = {};
		UiNodeHandle nextSibling = {};

		// 親の左上を原点とするローカル矩形
		UiRect localRect = {};

		UiNodeData data;

		uint32_t generation = 1;

		bool visible = true;
	};
}
