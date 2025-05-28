#pragma once
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <wrl.h>
#include <ResourceSystem/RootSignature/RootSignature2.h>
#include <ResourceSystem/Texture/TextureManager.h>

#include "Renderer/ConstantBuffer.h"

struct MatParam;
using Microsoft::WRL::ComPtr;

class Texture;
class Shader;

class Material {
public:
	Material(std::string name, Shader* shader);

	void SetTexture(const std::string& name, Texture* texture);
	void SetConstantBuffer(UINT shaderRegister, ID3D12Resource* buffer);

	void Apply(ID3D12GraphicsCommandList* commandList);

	ID3D12PipelineState* GetOrCreatePipelineState(
		ID3D12Device*                             device,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC& baseDesc);
	ID3D12RootSignature* GetOrCreateRootSignature(ID3D12Device* device);

	void InitializeRootSignature() const;

	void Shutdown();

	[[nodiscard]] const std::string& GetName() const;
	[[nodiscard]] Shader*            GetShader() const { return shader_; }
	[[nodiscard]] const std::unordered_map<std::string, Texture*>&
	GetTextures() const;

private:
	static std::string GenerateBufferKey(D3D12_SHADER_VISIBILITY visibility,
	                                     UINT                    bindPoint);

	std::string name_;   // マテリアルの名前
	Shader*     shader_; // シェーダ

	ComPtr<ID3D12PipelineState> pipelineState_; // キャッシュされたパイプラインステート
	ComPtr<ID3D12RootSignature> rootSignature_; // キャッシュされたルートシグネチャ
	std::unordered_map<std::string, Texture*> textures_; // テクスチャ 名前とテクスチャのペア

	std::unordered_map<std::string, ID3D12Resource*> constantBuffers_;
	// 定数バッファ キーはシェーダーステージ_レジスタ番号

	RootSignature2 rootSignatureBuilder_;
};
