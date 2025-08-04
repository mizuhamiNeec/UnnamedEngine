#pragma once

#ifdef _DEBUG
#include <imgui.h>
#else
#endif
#include <wrl/client.h>
#include <d3d12.h>

class SrvManager;
class D3D12;
class SceneComponent;
class ShaderResourceViewManager;
struct Vec3;

class ImGuiManager {
public:
	ImGuiManager(D3D12* renderer, const SrvManager* srvManager);
	static void NewFrame();
	void        EndFrame();
	void        Shutdown();
	void        Recreate() const;

#ifdef _DEBUG
	static void StyleColorsDark();
	static void StyleColorsLight();

	static void PushStyleColorForDrag(const ImVec4& bg, const ImVec4& bgHovered,
	                                  const ImVec4& bgActive);
	static bool EditTransform(SceneComponent& transform,
	                          const float&    vSpeed);
	static bool DragVec3(const std::string& name, Vec3& v, const float& vSpeed,
	                     const char*        format);
	static void TextOutlined(
		ImDrawList* drawList, const ImVec2& pos, const char* text,
		ImVec4      textColor, ImVec4       outlineColor,
		float       outlineSize = 1.0f
	);
	static bool IconButton(const char*   icon, const char* label,
	                       const ImVec2& size = ImVec2(0, 0));
#endif

private:
	D3D12*                                       mRenderer   = nullptr;
	const SrvManager*                            mSrvManager = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap;
};
