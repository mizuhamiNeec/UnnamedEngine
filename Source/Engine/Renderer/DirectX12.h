#pragma once
#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <format>
#include <DirectXTex/DirectXTex.h>
#include <wrl/client.h>

#include "Renderer.h"
#include "../Lib/Transform/Transform.h"
#include "../Lib/Math/Vector/Vec4.h"
#include "../Lib/Math/Vector/Vec2.h"
#include "../Lib/Math/Matrix/Mat4.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

using namespace Microsoft::WRL;

struct VertexData {
	Vec4 position;
	Vec2 texcoord;
	Vec3 normal;
};

struct Material {
	Vec4 color;
	int32_t enableLighting;
	float padding[3];
	Mat4 uvTransform;
};

struct TransformationMatrix {
	Mat4 wvp;
	Mat4 world;
};

struct DirectionalLight {
	Vec4 color; //!< ライトの色
	Vec3 direction; //!< ライトの向き
	float intensity; //!< 輝度
};

struct MaterialData {
	std::string textureFilePath;
};

struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};

ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

class DirectX12 final : public Renderer {
public:
	DirectX12();
	~DirectX12() override;

	// ==============================
	// 基本関数
	// ==============================
	void Init(Window* window) override;

	void PreRender() override;

	const unsigned int kFrameBufferCount = 2; // バッファリング数

	// ==============================
	// その他雑多関数
	// ==============================
	Transform TransformSprite() const;
	void SetTransformSprite(const Transform& transformSprite);

	Transform GetUvTransformSprite() const;
	void SetUvTransformSprite(const Transform& uvTransform);

	DirectionalLight* GetDirectionalLightData() const;

	VertexData* GetVertexDataSphere() const;

	void SetTransformationMatrixData(const TransformationMatrix& newtTransformationMatrix) const;

	void SetIsRunning(bool isRunning);

	void SetMaterialData(const Material& matData) const;
	Material* GetMaterialData() const;

	ID3D12Device* Device() const;

	DXGI_SWAP_CHAIN_DESC1 SwapChainDesc() const;

	D3D12_RENDER_TARGET_VIEW_DESC RtvDesc() const;

	ID3D12DescriptorHeap* SrvDescriptorHeap() const;

	static DirectX::ScratchImage LoadTexture(const std::string& filePath);

	static void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages);

	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap,
		uint32_t descriptorSize,
		uint32_t index);
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap,
		uint32_t descriptorSize,
		uint32_t index);

	bool* GetUseMonsterBall();

private:
	static void EnableDebugLayer();
	void CreateDevice();
	void CreateCommandQueue();
	void CreateCommandList();
	void CreateSwapChain();
	void GetResourceFromSwapChain();
	void CreateRTV();
	void InitializeDxc();

	void SetInfoQueueBreakOnSeverity() const;

	IDxcBlob* CompileShader(
		const std::wstring& filePath,
		const wchar_t* profile,
		IDxcUtils* dxcUtils,
		IDxcCompiler3* dxcCompiler,
		IDxcIncludeHandler* includeHandler
	);

	static ComPtr<ID3D12Resource> CreateTextureResource(ComPtr<ID3D12Device> device,
		const DirectX::TexMetadata& metadata);
	ComPtr<ID3D12Resource> CreateBufferResource(const ComPtr<ID3D12Device>& device, size_t sizeInBytes);
	ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(int32_t width, int32_t height) const;
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors,
		bool shaderVisible) const;

	HRESULT hr_ = 0;

	ComPtr<IDXGIFactory7> dxgiFactory_;
	ComPtr<ID3D12Device> device_;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc_ = {};
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc_ = {};

	ComPtr<ID3D12CommandQueue> commandQueue_;

	ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;

	ComPtr<IDXGISwapChain4> swapChain_;
	std::vector<ComPtr<ID3D12Resource>> swapChainResources_;
	ComPtr<ID3D12GraphicsCommandList> commandList_;

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = {};
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = {};
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = {};
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = {};

	ComPtr<ID3D12CommandAllocator> commandAllocator_;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2] = {}; // RTVを2つ作るのでディスクリプタを2つ用意

	ComPtr<ID3D12Fence> fence_;
	uint64_t fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;

	ComPtr<IDxcUtils> dxcUtils_;
	ComPtr<IDxcCompiler3> dxcCompiler_;
	ComPtr<IDxcIncludeHandler> includeHandler_;
	D3D12_VIEWPORT viewport_ = {};
	D3D12_RECT scissorRect_ = {};
	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12PipelineState> graphicsPipelineState_;
	ComPtr<ID3D12Resource> depthStencilResource;
	ComPtr<ID3DBlob> signatureBlob_;
	ComPtr<ID3DBlob> errorBlob_;
	ComPtr<IDxcBlob> vertexShaderBlob_;
	ComPtr<IDxcBlob> pixelShaderBlob_;

	// メッシュ
	ModelData modelData;

	ComPtr<ID3D12Resource> vertexResourceMesh_;
	ComPtr<ID3D12Resource> materialResourceMesh_;
	ComPtr<ID3D12Resource> transformationMatrixResourceMesh_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewMesh_ = {};
	TransformationMatrix* transformationMatrixDataMesh_ = nullptr;

	// Index用のあれやこれや
	ComPtr<ID3D12Resource> indexResourceSprite_;
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite_ = {};

	// スプライト
	ComPtr<ID3D12Resource> vertexResourceSprite_;
	ComPtr<ID3D12Resource> materialResourceSprite_;
	ComPtr<ID3D12Resource> transformationMatrixResourceSprite_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite_ = {};
	TransformationMatrix* transformationMatrixDataSprite_ = nullptr;
	Transform transformSprite_;
	Transform uvTransformSprite_;

	VertexData* vertexData = nullptr;
	VertexData* vertexDataSprite = nullptr;

	ComPtr<ID3D12Resource> textureResourceUvChecker_;
	ComPtr<ID3D12Resource> textureResourceMonsterBall_;

	// 指向性ライト
	ComPtr<ID3D12Resource> directionalLightResource_;
	DirectionalLight* directionalLightData_ = nullptr;

	Material* materialDataMesh_ = nullptr;
	Material* materialDataSprite_ = nullptr;

	uint32_t descriptorSizeSRV = 0;
	uint32_t descriptorSizeRTV = 0;
	uint32_t descriptorSizeDSV = 0;

	bool useMonsterBall = false;
};
