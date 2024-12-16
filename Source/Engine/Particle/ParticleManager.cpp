#include "ParticleManager.h"

#include <ranges>

#include "ParticleObject.h"

#include "../Camera/Camera.h"
#include "../Lib/Console/Console.h"
#include "../Lib/Math/Matrix/Mat4.h"
#include "../Lib/Math/Random/Random.h"
#include "../Renderer/D3D12.h"
#include "../Renderer/RootSignatureManager.h"
#include "../TextureManager/TextureManager.h"

void ParticleManager::Init(D3D12* d3d12, SrvManager* srvManager) {
	d3d12_ = d3d12;
	srvManager_ = srvManager;
	Console::Print("ParticleManager : ParticleCommonを初期化します。\n", kConsoleColorWait);
	// 頂点データの生成
	vertices_ = {
		{{1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, Vec3::forward},
		{{-1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, Vec3::forward},
		{{1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, Vec3::forward},
		{{1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, Vec3::forward},
		{{-1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, Vec3::forward},
		{{-1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, Vec3::forward}};

	// 頂点バッファの作成
	vertexBuffer_ = std::make_unique<VertexBuffer<Vertex>>(
		d3d12_->GetDevice(),
		sizeof(Vertex) * vertices_.size(),
		vertices_.data());

	CreateGraphicsPipeline();

	Console::Print("ParticleManager : ParticleCommonの初期化が完了しました。\n", kConsoleColorCompleted);
}

void ParticleManager::Shutdown() const {
}

void ParticleManager::CreateRootSignature() {
	//  RootSignatureManagerのインスタンスを作成
	rootSignatureManager_ = std::make_unique<RootSignatureManager>(d3d12_->GetDevice());

	D3D12_DESCRIPTOR_RANGE descriptorRange[1];
	descriptorRange[0] = {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV, // SRVを使う
		.NumDescriptors = 1, // 数は一つ
		.BaseShaderRegister = 0, // 0から始まる
		.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND // Offset
	};

	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
	descriptorRangeForInstancing[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRangeForInstancing[0].NumDescriptors = 1; // 数は1つ
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータを作成
	std::vector<D3D12_ROOT_PARAMETER> rootParameters(3);
	// マテリアル
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号0

	// ストラクチャードバッファー
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VertexShaderで使う
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing; // Tableの中身の配列を指定
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing); // Tableで利用する数

	// テクスチャ
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {
		{
			.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR, // バイリニアフィルタ
			.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP, // 0~1の範囲外をリピート
			.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER, // 比較しない
			.MaxLOD = D3D12_FLOAT32_MAX, // ありったけのMipmapを使う
			.ShaderRegister = 0, // レジスタ番号0を使う
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL // PixelShaderで使う
		},
	};

	// ルートシグネチャを作成
	rootSignatureManager_->CreateRootSignature("ParticleManager", rootParameters, staticSamplers, _countof(staticSamplers));

	if (rootSignatureManager_->Get("ParticleManager")) {
		Console::Print("ParticleManager : ルートシグネチャの生成に成功.\n", kConsoleColorCompleted);
	}
}

void ParticleManager::CreateGraphicsPipeline() {
	CreateRootSignature();

	pipelineState_ = PipelineState(D3D12_CULL_MODE_NONE, D3D12_FILL_MODE_SOLID, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	pipelineState_.SetInputLayout(Vertex::inputLayout);
	pipelineState_.SetRootSignature(rootSignatureManager_->Get("ParticleManager"));
	pipelineState_.SetBlendMode(kBlendModeAdd);
	pipelineState_.SetDepthWriteMask(D3D12_DEPTH_WRITE_MASK_ZERO);

	pipelineState_.SetVS(L"./Resources/Shaders/Particle.VS.hlsl");
	pipelineState_.SetPS(L"./Resources/Shaders/Particle.PS.hlsl");
	pipelineState_.Create(d3d12_->GetDevice());


	if (pipelineState_.Get()) {
		Console::Print("ParticleManager: パイプラインの作成に成功しました。\n");
	}
}

void ParticleManager::Update(float deltaTime) {
	// ビルボード行列の計算
	const Mat4 view = defaultCamera_->GetViewMat();
	const Mat4 projection = defaultCamera_->GetProjMat();

	// すべてのパーティクルグループについて処理する
	for (auto& particleGroup : particleGroups_ | std::views::values) {
		// グループ内のすべてのパーティクルについて処理する
		particleGroup.particles.remove_if([&](Particle& particle) {
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
			// ワールド行列を計算
			const Mat4 world = Mat4::Translate(particle.transform.translate) * Mat4::FromQuaternion(Quaternion::Euler(particle.transform.rotate)) * Mat4::Scale(particle.transform.scale);
			// ワールドビュープロジェクション行列を合成
			const Mat4 wvp = world * view * projection;
			// インスタンシング用データ1個分の書き込み
			particleGroup.instancingData->wvp = wvp;
			particleGroup.instancingData->world = world;
			particleGroup.instancingData->color = particle.color;
			++particleGroup.instancingData;
			return false; // パーティクルを保持
		});
	}
}

void ParticleManager::Render() {
	d3d12_->GetCommandList()->SetGraphicsRootSignature(rootSignatureManager_->Get("ParticleManager"));
	d3d12_->GetCommandList()->SetPipelineState(pipelineState_.Get());
	// プリミティブトポロジを設定
	d3d12_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// VBVを設定
	D3D12_VERTEX_BUFFER_VIEW vbView = vertexBuffer_->View();
	d3d12_->GetCommandList()->IASetVertexBuffers(0, 1, &vbView);
	// すべてのパーティクルグループについて
	// テクスチャのSRVのDescriptorTableを設定
	for (auto& particleGroup : particleGroups_ | std::views::values) {
		d3d12_->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(particleGroup.materialData.textureFilePath));
		// インスタンシングデータのSRVのDescriptorTableを設定
		d3d12_->GetCommandList()->SetGraphicsRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(particleGroup.srvIndex));
		// 描画
		d3d12_->GetCommandList()->DrawInstanced(static_cast<UINT>(vertices_.size()), particleGroup.numInstance, 0, 0);
	}
}

void ParticleManager::Emit(const std::string& name, const Vec3& pos, const uint32_t& count) {
	// 登録済みのパーティクルグループ名かチェックしてassert
	assert(!particleGroups_.contains(name));
	// 新たなパーティクルを作成し、指定されたパーティクルグループに登録
	for (uint32_t i = 0; i < count; ++i) {
		particleGroups_[name].particles.push_back(ParticleObject::MakeNewParticle(pos, ParticleObject::GenerateConeVelocity(30.0f), Vec3::zero, Vec3::zero));
	}
}

D3D12* ParticleManager::GetD3D12() const { return d3d12_; }


void ParticleManager::SetDefaultCamera(Camera* newCamera) {
	this->defaultCamera_ = newCamera;
}

Camera* ParticleManager::GetDefaultCamera() const {
	return defaultCamera_;
}

SrvManager* ParticleManager::GetSrvManager() const {
	return srvManager_;
}

const VertexBuffer<Vertex>* ParticleManager::GetVertexBuffer() const {
	return vertexBuffer_.get();
}

const std::vector<Vertex>& ParticleManager::GetVertices() const { return vertices_; }


void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath) {
	// 登録済みの名前かチェックしてアサート
	assert(!particleGroups_.contains(name));
	// 新たな空のパーティクルグループを作成し、コンテナに登録
	particleGroups_[name] = ParticleGroup();
	// 新たなパーティクルグループのマテリアルデータにテクスチャファイルパスを設定
	particleGroups_[name].materialData.textureFilePath = textureFilePath;
	// テクスチャを読み込む
	TextureManager::GetInstance()->LoadTexture(textureFilePath);
	// マテリアルデータにテクスチャのSRVインデックスを記録
	particleGroups_[name].materialData.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);
	// インスタンシング用リソースの生成
	particleGroups_[name].instancingResource = std::make_unique<ConstantBuffer>(
		d3d12_->GetDevice(),
		sizeof(ParticleForGPU) * kNumMaxInstance);
	// インスタンシング用にSRVを確保してSRVインデックスを記録
	particleGroups_[name].srvIndex = srvManager_->Allocate();
	// SRV生成(StructuredBuffer用設定)
	srvManager_->CreateSRVForStructuredBuffer(
		particleGroups_[name].srvIndex,
		particleGroups_[name].instancingResource->GetResource(),
		kNumMaxInstance,
		sizeof(ParticleForGPU));
}
