#include "ParticleManager.h"

#include <ranges>

#include <SubSystem/Console/Console.h>
#include <Lib/Math/Matrix/Mat4.h>
#include <Lib/Math/Random/Random.h>

#include <Renderer/D3D12.h>
#include <Renderer/RootSignatureManager.h>

#include <Camera/CameraManager.h>
#include <Particle/ParticleObject.h>

#include "SrvManager.h"

#include "TextureManager/TexManager.h"

void ParticleManager::Init(D3D12* d3d12, SrvManager* srvManager) {
	d3d12_      = d3d12;
	srvManager_ = srvManager;
	Console::Print("ParticleManager : ParticleCommonを初期化します。\n",
	               kConTextColorWait, Channel::Engine);
	// 頂点データの生成
	std::vector<Vertex> vertices_ = {
		{{1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, Vec3::forward},
		{{-1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, Vec3::forward},
		{{1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, Vec3::forward},
		{{1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, Vec3::forward},
		{{-1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, Vec3::forward},
		{{-1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, Vec3::forward}
	};

	std::vector<uint32_t> indices = {
		0, 1, 2,
		1, 5, 2
	};

	//
	// auto ringVertices = GenerateRingVertices(0.5f, 1.0f, 64);
	//
	// auto ringIndices = GenerateRingIndices(64);

	RegisterMesh(ParticleMeshType::Quad, vertices_, indices);

	CreateGraphicsPipeline();

	particleGroups_.clear();

	Console::Print("ParticleManager : ParticleCommonの初期化が完了しました。\n",
	               kConTextColorCompleted, Channel::Engine);
}

void ParticleManager::Shutdown() {
	Console::Print("ParticleManager : ParticleCommonを終了します。\n",
	               kConTextColorWait, Channel::Engine);

	// ルートシグネチャの解放
	if (rootSignatureManager_) {
		rootSignatureManager_.reset();
	}

	// パイプラインステートの開放
	if (pipelineState_) {
		pipelineState_.reset();
	}

	for (auto name : registeredGroupNames_) {
		particleGroups_[name].instancingResource.reset();
	}

	meshData_[ParticleMeshType::Quad].vertexBuffer.reset();
	meshData_[ParticleMeshType::Quad].indexBuffer.reset();

	meshData_[ParticleMeshType::Ring].vertexBuffer.reset();
	meshData_[ParticleMeshType::Ring].indexBuffer.reset();

	meshData_.clear();

	Console::Print("ParticleManager : ParticleCommonの終了が完了しました。\n",
	               kConTextColorCompleted, Channel::Engine);
}

void ParticleManager::CreateRootSignature() {
	//  RootSignatureManagerのインスタンスを作成
	rootSignatureManager_ = std::make_unique<RootSignatureManager>(
		d3d12_->GetDevice());

	D3D12_DESCRIPTOR_RANGE descriptorRange[1];
	descriptorRange[0] = {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV, // SRVを使う
		.NumDescriptors = 1,                          // 数は一つ
		.BaseShaderRegister = 0,                      // 0から始まる
		.OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND // Offset
	};

	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
	descriptorRangeForInstancing[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRangeForInstancing[0].NumDescriptors = 1; // 数は1つ
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	// SRVを使う
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータを作成
	std::vector<D3D12_ROOT_PARAMETER> rootParameters(3);
	// マテリアル
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号0

	// ストラクチャードバッファー
	rootParameters[1].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	// VertexShaderで使う
	rootParameters[1].DescriptorTable.pDescriptorRanges =
		descriptorRangeForInstancing; // Tableの中身の配列を指定
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(
		descriptorRangeForInstancing); // Tableで利用する数

	// テクスチャ
	rootParameters[2].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	// Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(
		descriptorRange);

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {
		{
			.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,   // バイリニアフィルタ
			.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP, // 0~1の範囲外をリピート
			.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,    // 比較しない
			.MaxLOD = D3D12_FLOAT32_MAX,                      // ありったけのMipmapを使う
			.ShaderRegister = 0,                              // レジスタ番号0を使う
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL // PixelShaderで使う
		},
	};

	// ルートシグネチャを作成
	rootSignatureManager_->CreateRootSignature(
		"ParticleManager", rootParameters, staticSamplers,
		_countof(staticSamplers)
	);

	if (rootSignatureManager_->Get("ParticleManager")) {
		Console::Print("ParticleManager : ルートシグネチャの生成に成功.\n",
		               kConTextColorCompleted, Channel::Engine);
	}
}

void ParticleManager::CreateGraphicsPipeline() {
	CreateRootSignature();

	// D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	// depthStencilDesc.DepthEnable = TRUE; // テストはON
	// depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 書き込まない!!!
	// depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	// depthStencilDesc.StencilEnable = FALSE;

	pipelineState_ = std::make_unique<PipelineState>(
		D3D12_CULL_MODE_NONE, D3D12_FILL_MODE_SOLID,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	pipelineState_->SetInputLayout(Vertex::inputLayout);
	pipelineState_->SetRootSignature(
		rootSignatureManager_->Get("ParticleManager"));
	pipelineState_->SetBlendMode(kBlendModeAdd);
	pipelineState_->SetDepthWriteMask(D3D12_DEPTH_WRITE_MASK_ZERO);

	pipelineState_->SetVS(L"./Resources/Shaders/Particle.VS.hlsl");
	pipelineState_->SetPS(L"./Resources/Shaders/Particle.PS.hlsl");
	pipelineState_->Create(d3d12_->GetDevice());

	if (pipelineState_->Get()) {
		Console::Print("ParticleManager: パイプラインの作成に成功しました。\n",
		               kConTextColorCompleted, Channel::Engine);
	}
}

void ParticleManager::Update(float deltaTime) {
	// すべてのパーティクルグループについて処理する
	for (auto& particleGroup : particleGroups_ | std::views::values) {
		// グループ内のすべてのパーティクルについて処理する
		particleGroup.particles.remove_if(
			[&](Particle& particle) {
				if (particle.currentTime >= particle.lifeTime) {
					return true; // パーティクルを削除
				}
				// 場の影響を計算(加速)
				particle.vel += particle.gravity * deltaTime;
				particle.vel -= particle.drag * particle.vel * deltaTime;
				// 移動処理(速度を座標に加算)
				particle.transform.translate += particle.vel * deltaTime;
				// 経過時間を計算
				particle.currentTime += deltaTime;
				return false; // パーティクルを保持
			}
		);
	}
}

void ParticleManager::Render() {
	// ビルボード行列の計算
	const Mat4 view       = CameraManager::GetActiveCamera()->GetViewMat();
	const Mat4 projection = CameraManager::GetActiveCamera()->GetProjMat();

	ID3D12DescriptorHeap* descriptorHeaps[] = {
		srvManager_->GetDescriptorHeap()
	};
	d3d12_->GetCommandList()->SetDescriptorHeaps(
		_countof(descriptorHeaps), descriptorHeaps);

	d3d12_->GetCommandList()->SetGraphicsRootSignature(
		rootSignatureManager_->Get("ParticleManager"));
	d3d12_->GetCommandList()->SetPipelineState(pipelineState_->Get());
	// プリミティブトポロジを設定
	d3d12_->GetCommandList()->IASetPrimitiveTopology(
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// すべてのパーティクルグループについて
	// テクスチャのSRVのDescriptorTableを設定
	for (auto& particleGroup : particleGroups_ | std::views::values) {
		// グループ内のすべてのパーティクルについて処理する
		for (auto& particle : particleGroup.particles) {
			// ワールド行列を計算
			const Mat4 world = Mat4::Translate(particle.transform.translate) *
				Mat4::FromQuaternion(
					Quaternion::Euler(particle.transform.rotate)
				) * Mat4::Scale(particle.transform.scale);
			// ワールドビュープロジェクション行列を合成
			const Mat4 wvp = world * view * projection;

			if (particleGroup.instancingData != nullptr) {
				// インスタンシング用データ1個分の書き込み
				particleGroup.instancingData->wvp   = wvp;
				particleGroup.instancingData->world = world;
				particleGroup.instancingData->color = particle.color;
				++particleGroup.instancingData;
			}
		}

		d3d12_->GetCommandList()->SetGraphicsRootDescriptorTable(
			2, TexManager::GetInstance()->GetSrvHandleGPU(
				particleGroup.materialData.textureFilePath)
		);
		// インスタンシングデータのSRVのDescriptorTableを設定
		d3d12_->GetCommandList()->SetGraphicsRootDescriptorTable(
			1, srvManager_->GetGPUDescriptorHandle(particleGroup.srvIndex)
		);

		// VBVとIBVを設定
		auto&                    meshData = GetMeshData(particleGroup.meshType);
		D3D12_VERTEX_BUFFER_VIEW vbView   = meshData.vertexBuffer->View();
		d3d12_->GetCommandList()->IASetVertexBuffers(0, 1, &vbView);

		// インデックスバッファを設定
		D3D12_INDEX_BUFFER_VIEW ibView = meshData.indexBuffer->View();
		d3d12_->GetCommandList()->IASetIndexBuffer(&ibView);

		// インデックスを使って描画
		d3d12_->GetCommandList()->DrawIndexedInstanced(
			static_cast<UINT>(meshData.indices.size()),
			particleGroup.numInstance,
			0, 0, 0
		);
	}
}

std::vector<Vertex> ParticleManager::GenerateRingVertices(
	float innerRadius, float outerRadius, int segments) {
	std::vector<Vertex> vertices;
	// 各セグメントごとに内側と外側の頂点を生成
	vertices.reserve(segments * 2);

	//float deltaAngle = 2.0f * std::numbers::pi_v<float> / segments;
	Vec3 normal = Vec3::forward;

	for (int i = 0; i < segments; ++i) {
		float theta = 2.0f * 3.14159265f * (float)i / (float)segments;
		float cosT  = cos(theta);
		float sinT  = sin(theta);

		// UV座標の計算
		float u = static_cast<float>(i) / segments;

		// 内側の頂点
		float x_in = cosT * innerRadius;
		float y_in = sinT * innerRadius;

		// 外側の頂点
		float x_out = cosT * outerRadius;
		float y_out = sinT * outerRadius;

		// 内側の頂点
		vertices.emplace_back(Vec4(x_in, y_in, 0.0f, 1.0f), Vec2(u, 0.0f),
		                      normal);
		// 外側の頂点
		vertices.emplace_back(Vec4(x_out, y_out, 0.0f, 1.0f), Vec2(u, 1.0f),
		                      normal);
	}

	return vertices;
}

std::vector<uint32_t> ParticleManager::GenerateRingIndices(int segments) {
	std::vector<uint32_t> indices;
	for (int i = 0; i < segments; ++i) {
		int next = (i + 1) % segments;
		// 2つのトライアングル
		indices.push_back(i * 2);     // outer
		indices.push_back(i * 2 + 1); // inner
		indices.push_back(next * 2);  // next outer

		indices.push_back(next * 2);     // next outer
		indices.push_back(i * 2 + 1);    // inner
		indices.push_back(next * 2 + 1); // next inner
	}
	return indices;
}

void ParticleManager::RegisterMesh(const ParticleMeshType       meshType,
                                   std::vector<Vertex>&         vertices,
                                   const std::vector<uint32_t>& indices) {
	MeshData meshData;
	meshData.vertices = vertices;
	meshData.indices  = indices;

	meshData.vertexBuffer = std::make_unique<VertexBuffer<Vertex>>(
		d3d12_->GetDevice(),
		static_cast<uint32_t>(vertices.size()) * sizeof(Vertex),
		vertices.data()
	);

	meshData.indexBuffer = std::make_unique<IndexBuffer>(
		d3d12_->GetDevice(),
		static_cast<uint32_t>(indices.size()) * sizeof(uint32_t),
		indices.data()
	);

	meshData_[meshType] = std::move(meshData);
}

MeshData& ParticleManager::GetMeshData(ParticleMeshType type) {
	return meshData_[type];
}

void ParticleManager::Emit(const std::string& name, const Vec3& pos,
                           const uint32_t&    count) {
	// パーティクルグループが存在しない場合は新規作成
	if (!particleGroups_.contains(name)) {
		particleGroups_[name] = ParticleGroup();
	}

	// 指定された数のパーティクルを追加
	for (uint32_t i = 0; i < count; ++i) {
		particleGroups_[name].particles.push_back(
			ParticleObject::MakeNewParticle(
				pos, ParticleObject::GenerateConeVelocity(30.0f), Vec3::zero,
				Vec3::zero, Vec4::white, Vec4::white, Vec3::one, Vec3::one)
		);
	}
}

D3D12* ParticleManager::GetD3D12() const {
	return d3d12_;
}

CameraComponent* ParticleManager::GetDefaultCamera() const {
	return CameraManager::GetActiveCamera().get();
}

SrvManager* ParticleManager::GetSrvManager() const {
	return srvManager_;
}

const VertexBuffer<Vertex>* ParticleManager::GetVertexBuffer() {
	return GetMeshData(ParticleMeshType::Quad).vertexBuffer.get();
}

const IndexBuffer* ParticleManager::GetIndexBuffer() {
	return GetMeshData(ParticleMeshType::Quad).indexBuffer.get();
}

const std::vector<Vertex>& ParticleManager::GetVertices() {
	return GetMeshData(ParticleMeshType::Quad).vertices;
}

const std::vector<uint32_t>& ParticleManager::GetIndices() {
	return GetMeshData(ParticleMeshType::Quad).indices;
}

void ParticleManager::CreateParticleGroup(const std::string& name,
                                          const std::string& textureFilePath) {
	// 登録済みの名前かチェックしてアサート
	assert(!particleGroups_.contains(name));
	// 新たな空のパーティクルグループを作成し、コンテナに登録
	particleGroups_[name] = ParticleGroup();
	// 新たなパーティクルグループのマテリアルデータにテクスチャファイルパスを設定
	particleGroups_[name].materialData.textureFilePath = textureFilePath;
	// テクスチャを読み込む
	TexManager::GetInstance()->LoadTexture(textureFilePath);
	// マテリアルデータにテクスチャのSRVインデックスを記録
	particleGroups_[name].materialData.textureIndex = TexManager::GetInstance()
		->GetTextureIndexByFilePath(
			textureFilePath
		);
	// インスタンシング用リソースの生成
	particleGroups_[name].instancingResource = std::make_unique<ConstantBuffer>(
		d3d12_->GetDevice(),
		sizeof(ParticleForGPU) * kNumMaxInstance,
		"ParticleInstancingResource"
	);
	// インスタンシング用にSRVを確保してSRVインデックスを記録
	particleGroups_[name].srvIndex = srvManager_->Allocate();
	// SRV生成(StructuredBuffer用設定)
	srvManager_->CreateSRVForStructuredBuffer(
		particleGroups_[name].srvIndex,
		particleGroups_[name].instancingResource->GetResource(),
		kNumMaxInstance,
		sizeof(ParticleForGPU)
	);

	registeredGroupNames_.emplace_back(name);
}
