#pragma once
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <wrl.h>

#include <engine/public/ResourceSystem/RootSignature/RootSignature2.h>

struct MatParam;

class Texture;
class Shader;

class Material {
public:
	Material(std::string name, Shader* shader);

	void SetTexture(const std::string& name, const std::string& filePath);
	void SetConstantBuffer(UINT shaderRegister, ID3D12Resource* buffer);

	// メッシュ名の設定・取得
	void                      SetMeshName(const std::string& meshName);
	[[nodiscard]] std::string GetMeshName() const;
	[[nodiscard]] std::string GetFullName() const; // メッシュ名_マテリアル名 を返す

	void Apply(ID3D12GraphicsCommandList* commandList,
	           const std::string&         meshName = "");

	ID3D12PipelineState* GetOrCreatePipelineState(
		ID3D12Device*                             device,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC& baseDesc,
		const std::string&                        meshName = "");
	ID3D12RootSignature* GetOrCreateRootSignature(ID3D12Device* device);

	void InitializeRootSignature() const;

	void Shutdown();

	[[nodiscard]] const std::string& GetName() const;
	[[nodiscard]] Shader*            GetShader() const { return mShader; }
	[[nodiscard]] const std::unordered_map<std::string, std::string>&
	GetTextures() const;

private:
	static std::string GenerateBufferKey(D3D12_SHADER_VISIBILITY visibility,
	                                     UINT                    bindPoint);

	std::string mName;    // マテリアルの名前
	std::string mEshName; // 関連付けられたメッシュの名前
	Shader*     mShader;  // シェーダ

	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState;
	// キャッシュされたパイプラインステート
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
	// キャッシュされたルートシグネチャ
	std::unordered_map<std::string, std::string> mTextures;
	// テクスチャ 名前とファイルパスのペア

	std::unordered_map<std::string, ID3D12Resource*> mConstantBuffers;
	// 定数バッファ キーはシェーダーステージ_レジスタ番号

	RootSignature2 mRootSignatureBuilder;
};
