#pragma once

#include "../Renderer/D3D12.h"

#include "Structs/Structs.h"

#ifdef _DEBUG
#include "imgui/imgui.h"
#else
struct ImVec4 {
	float x, y, z, w;
};
#endif

struct ColorTransition {
	ImVec4 targetColor;
	bool isTransitioning;
};

class ImGuiManager {
public:
	void Init(const D3D12* renderer, const Window* window);
	static void NewFrame();
	void EndFrame() const;
	void Shutdown();

private:
	bool isDarkMode = false;

	std::vector<ColorTransition> colorTransitions;

	const D3D12* renderer_ = nullptr;
	ComPtr<ID3D12DescriptorHeap> srvHeap_;
};

#ifdef _DEBUG
void PushStyleColorForDrag(const ImVec4& bg, const ImVec4& bgHovered, const ImVec4& bgActive);
void EditTransform(const std::string& name, Transform& transform, const float& vSpeed);
void TextOutlined(ImDrawList* drawList, const ImVec2& pos, const char* text, ImU32 textColor, ImU32 outlineColor,
                  float outlineSize = 1.0f);
#endif
