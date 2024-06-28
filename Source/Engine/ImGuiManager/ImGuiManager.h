#pragma once

#ifdef _DEBUG
#include "../Renderer/D3D12.h"
#include "imgui/imgui.h"

struct ColorTransition {
	ImVec4 targetColor;
	bool isTransitioning;
};

class ImGuiManager {
public:
	void Initialize(const D3D12* renderer, const Window* window);
	void NewFrame();
	void EndFrame() const;
	void Shutdown();

private:
	bool isDarkMode = false;

	std::vector<ColorTransition> colorTransitions;

	const D3D12* renderer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;
};

#endif