#include "GameScene.h"

#include <format>

#include "../../Shared/Math/Vector/Vec3.h"
#include "../Engine/Renderer/PipelineState.h"
#include "../Engine/Renderer/RootSignature.h"
#include "../Engine/Renderer/VertexBuffer.h"
#include "../Engine/TextureManager/TextureManager.h"
#include "../Engine/Utils/Logger.h"
#include "../Shared/Math/MathLib.h"
#include "../Shared/Structs/Structs.h"
#include "imgui/imgui.h"

VertexBuffer* vertexBuffer;
ConstantBuffer* transformation;
ConstantBuffer* materialResource;
Material* material;

RootSignature* rootSignature;
PipelineState* pipelineState;

std::shared_ptr<Texture> texture;
std::shared_ptr<Texture> texture2;

D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU;
D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU;

void GameScene::Startup(D3D12* renderer, Window* window) {
	renderer_ = renderer;
	window_ = window;

	transform = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	cameraTransform = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, -1.0f}
	};

#pragma region 頂点バッファ
	Vertex vertices[3] = {};
	// 左下
	vertices[0].position = { -0.5f,-0.5f,0.0f ,1.0f };
	vertices[0].normal = { 0.0f,0.0f,-1.0f };
	vertices[0].texcoord = { 0.0f,1.0f };

	// 上
	vertices[1].position = { 0.0f,0.5f,0.0f ,1.0f };
	vertices[1].normal = { 0.0f,0.0f,-1.0f };
	vertices[1].texcoord = { 0.5f,0.0f };

	// 右下
	vertices[2].position = { 0.5f,-0.5f,0.0f ,1.0f };
	vertices[2].normal = { 0.0f,0.0f,-1.0f };
	vertices[2].texcoord = { 1.0f,1.0f };

	size_t vertexStride = sizeof(Vertex);
	vertexBuffer = new VertexBuffer(renderer_->GetDevice(), sizeof(Vertex) * 3, vertexStride, vertices);

	if (vertexBuffer) {
		Log("頂点バッファの生成に成功.\n");
	}
#pragma endregion

#pragma region 定数バッファ

	transformation = new ConstantBuffer(renderer_->GetDevice(), sizeof(TransformationMatrix));

	materialResource = new ConstantBuffer(renderer_->GetDevice(), sizeof(Material));
	// マテリアルにデータを書き込む
	material = materialResource->GetPtr<Material>(); // 書き込むためのアドレスを取得
	*material = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白
	material->enableLighting = false;
	material->uvTransform = Mat4::Identity();
#pragma endregion

#pragma region ルートシグネチャ
	rootSignature = new RootSignature(renderer->GetDevice());
	if (rootSignature) {
		Log("ルートシグネチャの生成に成功.\n");
	}
#pragma endregion

#pragma region パイプラインステート
	pipelineState = new PipelineState();
	pipelineState->SetInputLayout(Vertex::inputLayout);
	pipelineState->SetRootSignature(rootSignature->Get());
	pipelineState->SetVS(L"./Resources/Shaders/Object3d.VS.hlsl");
	pipelineState->SetPS(L"./Resources/Shaders/Object3d.PS.hlsl");
	pipelineState->Create(renderer->GetDevice());
	if (pipelineState) {
		Log("パイプラインステートの生成に成功.\n");
	}
#pragma endregion
	// テクスチャのロードとSRVハンドルの取得
	texture = TextureManager::GetInstance().GetTexture(renderer_->GetDevice(), L"./Resources/Textures/Debugempty.png");
	texture2 = TextureManager::GetInstance().GetTexture(renderer_->GetDevice(), L"./Resources/Textures/uvChecker.png");
}

void GameScene::Update() {
	transform.rotate.y += 0.003f;
	Mat4 worldMat = Mat4::Affine(transform.scale, transform.rotate, transform.translate);
	Mat4 cameraMat = Mat4::Affine(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
	Mat4 viewMat = cameraMat.Inverse();
	Mat4 projectionMat = Mat4::PerspectiveFovMat(
		90.0f * deg2Rad, // FieldOfView 90 degree!!
		static_cast<float>(window_->GetWindowConfig().clientWidth) / static_cast<float>(window_->GetWindowConfig().clientHeight),
		0.1f,
		100.0f
	);
	Mat4 worldViewProjMat = worldMat * viewMat * projectionMat;

	TransformationMatrix* ptr = transformation->GetPtr<TransformationMatrix>();
	ptr->WVP = worldViewProjMat;
	ptr->World = worldMat;
}

void GameScene::Render() {
	//size_t currentIndex = renderer_->GetBackBufferCount();
	ID3D12GraphicsCommandList* commandList = renderer_->GetCommandList();
	D3D12_VERTEX_BUFFER_VIEW vbView = vertexBuffer->View();

	// ディスクリプタヒープの設定
	ID3D12DescriptorHeap* descriptorHeaps[] = { texture2->GetSRVHeap() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	commandList->SetGraphicsRootSignature(rootSignature->Get());
	commandList->SetPipelineState(pipelineState->Get());

	commandList->IASetVertexBuffers(0, 1, &vbView);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetAddress());
	commandList->SetGraphicsRootConstantBufferView(1, transformation->GetAddress());
	commandList->SetGraphicsRootDescriptorTable(2, texture2->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart()); // テクスチャのSRVを設定

	commandList->DrawInstanced(3, 1, 0, 0);
}

void GameScene::Shutdown() {
	texture.reset();
	texture2.reset();

	delete vertexBuffer;
	delete transformation;
	delete materialResource;
	delete rootSignature;
	delete pipelineState;
}

