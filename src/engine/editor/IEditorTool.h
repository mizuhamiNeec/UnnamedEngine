#pragma once
#ifdef _DEBUG

#include <string>
#include <string_view>
#include <vector>

#include "core/math/Vec2.h"

#include "engine/ImGui/Icons.h"

namespace Unnamed {
	class ConsoleSystem;
	class InputSystem;
	class AssetManager;
	class IDemoService;
	class IGameWorldFactory;
	class Profiler;
	class ImGuiLayer;
	class WindowManager;
	class World;

	namespace Render {
		class RenderModule;
		struct RenderFrameInputs;
		struct SceneOutputView;
	}

	struct EditorToolServices {
		WindowManager*        windowManager    = nullptr;
		Render::RenderModule* renderModule     = nullptr;
		ImGuiLayer*           imGuiLayer       = nullptr;
		ConsoleSystem*        console          = nullptr;
		InputSystem*          inputSystem      = nullptr;
		AssetManager*         assetManager     = nullptr;
		IDemoService*         demoService      = nullptr;
		IGameWorldFactory*    gameWorldFactory = nullptr;
		Profiler*             profiler         = nullptr;
	};

	struct EditorToolFrameContext {
		float unscaledDeltaTime = 0.0f;
		float deltaTime         = 0.0f;
	};

	class IEditorTool {
	public:
		virtual ~IEditorTool() = default;

		[[nodiscard]] virtual std::string_view GetToolId() const = 0;
		[[nodiscard]] virtual std::string_view GetDisplayName() const = 0;

		virtual void Initialize(const EditorToolServices& services) {
			(void)services;
		}

		virtual void Shutdown() {
		}

		virtual void BeginUI() {
		}

		virtual void Tick(const EditorToolFrameContext& frameContext) = 0;
		virtual void BuildUi(const EditorToolFrameContext& frameContext) = 0;

		virtual void CollectRenderViews(Render::RenderFrameInputs& inputs) = 0;
		virtual void EnumerateViewKeys(
			std::vector<std::string>& outViewKeys
		) const = 0;
		virtual void SetViewOutput(
			std::string_view               viewKey,
			const Render::SceneOutputView& output,
			Vec2                           size
		) = 0;

		[[nodiscard]] virtual World* GetRuntimeWorld() {
			return nullptr;
		}

		[[nodiscard]] virtual bool IsOpen() const = 0;
		virtual void               SetOpen(bool open) = 0;

		virtual uint32_t GetIcon() {
			return kIconQuestionMark;
		}
	};
}
#endif
