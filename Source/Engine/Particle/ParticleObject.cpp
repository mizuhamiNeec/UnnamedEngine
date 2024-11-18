#include "ParticleObject.h"
#include "ParticleCommon.h"

#include "../Camera/Camera.h"

#include "../ImGuiManager/ImGuiManager.h"

#include "../Renderer/D3D12.h"

#include "../TextureManager/TextureManager.h"

#include "../Lib/Math/Random/Random.h"

#ifdef _DEBUG
#include "imgui/imgui.h"
#endif

void ParticleObject::Init(ParticleCommon* particleCommon, const std::string& textureFilePath) {
	this->particleCommon_ = particleCommon;
	this->textureFilePath_ = textureFilePath;
	this->camera_ = particleCommon_->GetDefaultCamera();

	particles_.resize(kNumMaxInstance);

	// 各トランスフォームに初期値を設定
	for (Particle& particle : particles_) {
		particle = MakeNewParticle(Vec3::zero);
	}

	// 頂点リソースにデータを書き込む
	vertices_.push_back({ {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, Vec3::forward }); // 左上
	vertices_.push_back({ {-1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, Vec3::forward }); // 右上
	vertices_.push_back({ {1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, Vec3::forward }); // 左下
	vertices_.push_back({ {1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, Vec3::forward }); // 左下
	vertices_.push_back({ {-1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, Vec3::forward }); // 右上
	vertices_.push_back({ {-1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, Vec3::forward }); // 右下

	uint32_t indices[] = {
		0, 1, 2,
		1, 5, 2
	};

	// インデックスバッファの作成
	indexBuffer_ = std::make_unique<IndexBuffer>(
		particleCommon_->GetD3D12()->GetDevice(),
		sizeof(indices),
		indices
	);

	// 頂点バッファの作成
	vertexBuffer_ = std::make_unique<VertexBuffer>(
		particleCommon_->GetD3D12()->GetDevice(),
		sizeof(Vertex) * vertices_.size(),
		sizeof(Vertex),
		vertices_.data()
	);

	// 定数バッファ
	materialResource_ = std::make_unique<ConstantBuffer>(particleCommon_->GetD3D12()->GetDevice(), sizeof(Material));
	materialData_ = materialResource_->GetPtr<Material>();
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->enableLighting = false;
	materialData_->uvTransform = Mat4::IdentityMat();

	// Instancing用のTransformationMatrixリソースを作る
	instancingResource_ = std::make_unique<ConstantBuffer>(
		particleCommon_->GetD3D12()->GetDevice(), sizeof(ParticleForGPU) * kNumMaxInstance);
	// 書き込むためのアドレスを取得
	instancingData = instancingResource_->GetPtr<ParticleForGPU>();
	// 単位行列を書き込んでおく
	for (uint32_t index = 0; index < kNumMaxInstance; ++index) {
		instancingData[index].wvp = Mat4::IdentityMat();
		instancingData[index].world = Mat4::IdentityMat();
		instancingData[index].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	}

	// SrvManagerのインスタンスを取得
	srvManager_ = particleCommon_->GetSrvManager();

	// SRVのインデックスを取得
	srvIndex_ = srvManager_->Allocate();

	// StructuredBuffer用のSRVを作成
	srvManager_->CreateSRVForStructuredBuffer(
		srvIndex_,
		instancingResource_->GetResource(), // ID3D12Resource* 型のリソース
		kNumMaxInstance, // 要素数
		sizeof(TransformationMatrix) // 構造体のバイトサイズ
	);

	emitter_.transform = { Vec3::one, Vec3::zero, Vec3::zero };

	emitter_.count = 3;
	emitter_.frequency = 0.5f; // 0.5秒ごとに発生
	emitter_.frequencyTime = 0.0f; // 発生頻度用の時刻、0で初期化

	accelerationField_.acceleration = { 15.0f, 0.0f, 0.0f };
	accelerationField_.area.min = { -1.0f, -1.0f, -1.0f };
	accelerationField_.area.max = { 1.0f, 1.0f, 1.0f };
}

void ParticleObject::Update(const float deltaTime) {
#ifdef _DEBUG
	ImGui::Begin("Particle");
	ImGui::Text("Particle Instance : %u", particles_.size());
	EditTransform("EmitterTranslate", emitter_.transform, 0.1f);
	if (ImGui::Button("Add Particle")) {
		particles_.splice(particles_.end(), Emit(emitter_));
	}
	ImGui::End();
#endif

	numInstance = 0;
	for (std::list<Particle>::iterator particleIterator = particles_.begin();
		particleIterator != particles_.end();) {
		if (particleIterator->lifeTime <= particleIterator->currentTime) {
			particleIterator = particles_.erase(particleIterator); // 生存期間が過ぎたParticleはlistから消す。戻り値が次のイテレータとなる
			continue;
		}

		if (numInstance < kNumMaxInstance) {
			// Fieldの範囲内のParticleには加速度を適用する
			if (Math::IsCollision(accelerationField_.area, particleIterator->transform.translate)) {
				particleIterator->vel += accelerationField_.acceleration * deltaTime;
			}

			// 速度を適用
			particleIterator->transform.translate += particleIterator->vel * deltaTime;
			particleIterator->currentTime += deltaTime;

			float lifeRatio = particleIterator->currentTime / particleIterator->lifeTime;
			instancingData[numInstance].color = particleIterator->color;
			instancingData[numInstance].color.w = 1.0f - lifeRatio;

			Mat4 worldMat;

			// ビルボード
			{
				Mat4 cameraMat = Mat4::Affine(Vec3::one, camera_->GetRot(), camera_->GetPos());
				Mat4 backToFrontMat = Mat4::RotateY(std::numbers::pi_v<float>);
				Mat4 billboardMatrix = backToFrontMat * cameraMat;
				billboardMatrix.m[3][0] = 0.0f;
				billboardMatrix.m[3][1] = 0.0f;
				billboardMatrix.m[3][2] = 0.0f;
				worldMat = Mat4::ScaleMat(particleIterator->transform.scale) * billboardMatrix * Mat4::TranslateMat(
					particleIterator->transform.translate);
			}

			// Mat4 worldMat = Mat4::Affine(
			// 	it->transform.scale,
			// 	it->transform.rotate,
			// 	it->transform.translate
			// );

			Mat4 worldViewProjMat;

			if (camera_) {
				// カメラが存在する場合はカメラから行列を持ってくる
				const Mat4& viewProjMat = camera_->GetViewProjMat();
				worldViewProjMat = worldMat * viewProjMat;
			} else {
				worldViewProjMat = worldMat;
			}

			instancingData[numInstance].wvp = worldViewProjMat;
			instancingData[numInstance].world = worldMat;

			++numInstance;
		}
		++particleIterator; // 次のイテレータに進める
	}

	emitter_.frequencyTime += deltaTime; // 時刻を進める
	// 頻度より大きいなら発生
	if (emitter_.frequency <= emitter_.frequencyTime) {
		particles_.splice(particles_.end(), Emit(emitter_)); // 発生処理
		emitter_.frequencyTime -= emitter_.frequency; // 余計に過ぎた時間も加味して頻度計算する
	}
}

void ParticleObject::Draw() const {
	// 頂点バッファの設定
	D3D12_VERTEX_BUFFER_VIEW vbView = vertexBuffer_->View();
	particleCommon_->GetD3D12()->GetCommandList()->IASetVertexBuffers(0, 1, &vbView);

	// 定数バッファの設定
	particleCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootConstantBufferView(
		0, materialResource_->GetAddress());

	// SRVを設定
	particleCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootDescriptorTable(
		1, srvManager_->GetGPUDescriptorHandle(srvIndex_));

	// SRVのDescriptorTableの先頭を設定
	particleCommon_->GetD3D12()->GetCommandList()->SetGraphicsRootDescriptorTable(
		2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));

	// インデックスバッファの設定
	D3D12_INDEX_BUFFER_VIEW indexBufferView = indexBuffer_->View();
	particleCommon_->GetD3D12()->GetCommandList()->IASetIndexBuffer(&indexBufferView);
	// 描画
	particleCommon_->GetD3D12()->GetCommandList()->DrawIndexedInstanced(
		static_cast<UINT>(vertices_.size()), numInstance, 0, 0, 0);
}

Particle ParticleObject::MakeNewParticle(const Vec3& pos) {
	Random* random = Random::GetInstance();
	Particle particle;
	particle.transform.scale = { 1.0f, 1.0f, 1.0f };
	particle.transform.rotate = { 0.0f, 0.0f, 0.0f };
	Vec3 rand = {
		random->RandomFloat(-1.0f, 1.0f),
		random->RandomFloat(-1.0f, 1.0f),
		random->RandomFloat(-1.0f, 1.0f)
	};
	particle.transform.translate = pos + rand;

	// 速度を上向きに設定
	particle.vel = {
		random->RandomFloat(-1.0f, 1.0f),
		random->RandomFloat(-1.0f, 1.0f),
		random->RandomFloat(-1.0f, 1.0f)
	};
	// ノーマライズしておく
	particle.vel.Normalize();

	// 色
	particle.color = {
		random->RandomFloat(0.0f, 1.0f), random->RandomFloat(0.0f, 1.0f), random->RandomFloat(0.0f, 1.0f), 1.0f
	};

	// 生存時間
	particle.lifeTime = random->RandomFloat(1.0f, 4.0f);
	particle.currentTime = 0.0f;

	return particle;
}

std::list<Particle> ParticleObject::Emit(const Emitter& emitter) {
	std::list<Particle> particles;
	for (uint32_t count = 0; count < emitter.count; ++count) {
		particles.push_back(MakeNewParticle(emitter.transform.translate));
	}
	return particles;
}

// Vec3 Particle::GetPos() const { return transform_.translate; }
// Vec3 Particle::GetScale() const { return transform_.scale; }
//
// void Particle::SetPos(const Vec3& newPos) { transform_.translate = newPos; }
// void Particle::SetScale(const Vec3& newScale) { transform_.scale = newScale; }
void ParticleObject::SetCamera(Camera* newCamera) { camera_ = newCamera; }
