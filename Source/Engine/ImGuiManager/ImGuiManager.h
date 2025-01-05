#pragma once

#include "../Renderer/D3D12.h"

#include "../Lib/Structs/Structs.h"

#ifdef _DEBUG
#include <imgui.h>
#else
#endif

class TransformComponent;
class SrvManager;

struct ColorTransition {
	Vec4 targetColor;
	bool isTransitioning;
};

class ImGuiManager {
public:
	void Init(const D3D12* renderer, const Window* window, const SrvManager* srvManager);
	static void NewFrame();
	void EndFrame() const;
	void Shutdown();

#ifdef _DEBUG
	static void PushStyleColorForDrag(const ImVec4& bg, const ImVec4& bgHovered, const ImVec4& bgActive);
	static bool EditTransform(Transform& transform, const float& vSpeed);
	static bool EditTransform(TransformComponent& transform, const float& vSpeed);
	static bool DragVec3(const std::string& name, Vec3& v, const float& vSpeed, const char* format);
	static void TextOutlined(
		ImDrawList* drawList, const ImVec2& pos, const char* text, ImU32 textColor, ImU32 outlineColor,
		float outlineSize = 1.0f
	);
#endif

private:
#ifdef _DEBUG
	static void StyleColorsDark(ImGuiStyle* dst = nullptr);
#endif

	const D3D12* renderer_ = nullptr;
	const SrvManager* srvManager_ = nullptr;
	ComPtr<ID3D12DescriptorHeap> srvHeap_;
};
