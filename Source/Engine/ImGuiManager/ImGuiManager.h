#pragma once

#include "../Renderer/D3D12.h"

#ifdef _DEBUG
#include "imgui/imgui.h"
#endif

#include "../../../Console.h"

struct ColorTransition {
	ImVec4 targetColor;
	bool isTransitioning;
};

class ImGuiManager {
public:
	void Init(const D3D12* renderer, const Window* window);
	void NewFrame();
	void EndFrame() const;
	void Shutdown();

private:
	bool isDarkMode = false;

	std::vector<ColorTransition> colorTransitions;

	const D3D12* renderer_ = nullptr;
	ComPtr<ID3D12DescriptorHeap> srvHeap_;
};

#ifdef _DEBUG
inline void TextOutlined(ImDrawList* drawList, const ImVec2& pos, const char* text, ImU32 textColor, ImU32 outlineColor, float outlineSize = 1.0f) {
	drawList->AddText(ImVec2(pos.x - outlineSize, pos.y), outlineColor, text);
	drawList->AddText(ImVec2(pos.x + outlineSize, pos.y), outlineColor, text);
	drawList->AddText(ImVec2(pos.x, pos.y - outlineSize), outlineColor, text);
	drawList->AddText(ImVec2(pos.x, pos.y + outlineSize), outlineColor, text);
	drawList->AddText(ImVec2(pos.x - outlineSize, pos.y - outlineSize), outlineColor, text);
	drawList->AddText(ImVec2(pos.x + outlineSize, pos.y - outlineSize), outlineColor, text);
	drawList->AddText(ImVec2(pos.x - outlineSize, pos.y + outlineSize), outlineColor, text);
	drawList->AddText(ImVec2(pos.x + outlineSize, pos.y + outlineSize), outlineColor, text);
	drawList->AddText(pos, textColor, text);
}
#endif