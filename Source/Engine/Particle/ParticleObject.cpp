#include "ParticleObject.h"

#include <Engine.h>
#include <Camera/CameraManager.h>
#include <Debug/Debug.h>
#include <ImGuiManager/ImGuiManager.h>
#include <Lib/Math/Quaternion/Quaternion.h>
#include <Lib/Math/Random/Random.h>
#include <Particle/ParticleManager.h>
#include <Renderer/D3D12.h>

#include "TextureManager/TexManager.h"

void ParticleObject::Init(ParticleManager*   particleCommon,
                          const std::string& textureFilePath) {
	this->particleCommon_  = particleCommon;
	this->textureFilePath_ = textureFilePath;
	this->camera_          = particleCommon_->GetDefaultCamera();

	// particles_.resize(kNumMaxInstance);
	//
	// // 各トランスフォームに初期値を設定
	// for (Particle& particle : particles_) {
	// 	particle = MakeNewParticle(Vec3::zero, Vec3::zero, Vec3::zero,
	// 	                           Vec3::zero, , TODO);
	// }

	std::vector<uint32_t> indices = {
		0, 1, 2,
		1, 5, 2
	};

	// インデックスバッファの作成
	indexBuffer_ = std::make_unique<IndexBuffer>(
		particleCommon_->GetD3D12()->GetDevice(),
		indices.size() * sizeof(uint32_t),
		indices.data()
	);

	// 定数バッファ
	materialResource_ = std::make_unique<ConstantBuffer>(
		particleCommon_->GetD3D12()->GetDevice(),
		sizeof(Material),
		"ParticleMaterial"
	);
	materialData_                 = materialResource_->GetPtr<Material>();
	materialData_->color          = {1.0f, 1.0f, 1.0f, 1.0f};
	materialData_->enableLighting = false;
	materialData_->uvTransform    = Mat4::identity;

	// Instancing用のTransformationMatrixリソースを作る
	instancingResource_ = std::make_unique<ConstantBuffer>(
		particleCommon_->GetD3D12()->GetDevice(),
		sizeof(ParticleForGPU) * kNumMaxInstance,
		"ParticleInstancing"
	);
	// 書き込むためのアドレスを取得
	instancingData = instancingResource_->GetPtr<ParticleForGPU>();
	// 単位行列を書き込んでおく
	for (uint32_t index = 0; index < kNumMaxInstance; ++index) {
		instancingData[index].wvp   = Mat4::identity;
		instancingData[index].world = Mat4::identity;
		instancingData[index].color = {1.0f, 1.0f, 1.0f, 1.0f};
	}

	// SrvManagerのインスタンスを取得
	srvManager_ = particleCommon_->GetSrvManager();

	// SRVのインデックスを取得
	srvIndex_ = srvManager_->AllocateForStructuredBuffer();

	// StructuredBuffer用のSRVを作成
	srvManager_->CreateSRVForStructuredBuffer(
		srvIndex_,
		instancingResource_->GetResource(), // ID3D12Resource* 型のリソース
		kNumMaxInstance,                    // 要素数
		sizeof(TransformationMatrix)        // 構造体のバイトサイズ
	);

	emitter_.transform = {Vec3::one, Vec3::zero, Vec3::zero};

	emitter_.count         = 3;
	emitter_.frequency     = 0.5f;      // 0.5秒ごとに発生
	emitter_.frequencyTime = 0.0f;      // 発生頻度用の時刻、0で初期化
	emitter_.size          = Vec3::one; // エミッターのサイズを初期化

	//accelerationField_ = AccelerationField({ 15.0f, 0.0f, 0.0f }, { {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f} });
}

void ParticleObject::Update(const float deltaTime) {
	static int   shapeType = 0;
	static float coneAngle = 45.0f;
	static Vec3  drag      = {0.0f, 0.0f, 0.0f};
	static Vec3  gravity   = {0.0f, -9.8f, 0.0f};
#ifdef _DEBUG
	ImGui::Begin(("Particle" + textureFilePath_).c_str());
	ImGui::Text("Particle Instance : %u", particles_.size());

	// エミッタのトランスフォームを編集
	ImGuiManager::EditTransform(emitter_.transform, 0.1f);

	// ビルボードタイプの設定
	const char* billboardTypes[] = {"None", "XY", "XZ", "YZ", "All"};
	ImGui::Combo("Billboard Type", reinterpret_cast<int*>(&billboardType_),
	             billboardTypes, IM_ARRAYSIZE(billboardTypes));

	// 発生数と頻度の設定
	ImGui::InputInt("Particle Count", reinterpret_cast<int*>(&emitter_.count));
	ImGui::DragFloat("Emission Frequency", &emitter_.frequency);

	// エミッション形状の選択
	const char* shapeItems[] = {"Sphere", "Cube"};
	ImGui::Combo("Emission Shape", &shapeType, shapeItems,
	             IM_ARRAYSIZE(shapeItems));
	// コーン速度の設定
	ImGui::SliderAngle("Cone Angle", &coneAngle, 0.0f, 90.0f);

	// 抗力と重力の設定
	ImGui::DragFloat3("Drag", &drag.x);
	ImGui::DragFloat3("Gravity", &gravity.x);

	// 物理特性のオン・オフ設定
	ImGui::Checkbox("Enable Gravity", &enableGravity);
	ImGui::Checkbox("Enable Velocity", &enableAccelerationField_);
	ImGui::Checkbox("Enable Drag", &enableDrag);

	// AccelerationFieldのパラメーター編集
	ImGui::DragFloat3("Acceleration", &accelerationField_.acceleration.x);

	// エミッターのサイズを編集
	ImGui::DragFloat3("Emitter Size", &emitter_.size.x);

	if (ImGui::Button("Emit Particles")) {
		particles_.splice(particles_.end(),
		                  Emit(emitter_, shapeType, coneAngle, drag, gravity,
		                       Vec3::zero, Vec4::white, Vec4::white, Vec3::one,
		                       Vec3::one));
	}

	for (auto particle : particles_) {
		ImGui::Text("Particle Position : %.2f %.2f %.2f",
		            particle.transform.translate.x,
		            particle.transform.translate.y,
		            particle.transform.translate.z);
		ImGui::Text("Particle Velocity : %.2f %.2f %.2f", particle.vel.x,
		            particle.vel.y, particle.vel.z);
		ImGui::Text("Particle LifeTime : %.2f", particle.lifeTime);
	}

	ImGui::End();
#endif

	numInstance = 0;
	for (auto particleIterator = particles_.begin();
	     particleIterator != particles_.end();) {
		if (particleIterator->lifeTime <= particleIterator->currentTime) {
			particleIterator = particles_.erase(particleIterator);
			// 生存期間が過ぎたParticleはlistから消す。戻り値が次のイテレータとなる
			continue;
		}

		if (numInstance < kNumMaxInstance) {
			if (enableAccelerationField_) {
				// Fieldの範囲内のParticleには加速度を適用する
				particleIterator->vel += accelerationField_.acceleration *
					deltaTime;
			}

			// 重力の適用
			if (enableGravity) {
				particleIterator->vel += particleIterator->gravity * deltaTime;
			}

			// 抗力の適用
			if (enableDrag) {
				particleIterator->vel -= particleIterator->drag *
					particleIterator->vel * deltaTime;
			}

			// 速度を適用
			particleIterator->transform.translate += particleIterator->vel *
				deltaTime;


			// 速度によってy軸方向に拡大
			//particleIterator->transform.scale.x = particleIterator->vel.y * 0.1f + 0.25f;

			particleIterator->currentTime += deltaTime;

			float lifeRatio = particleIterator->currentTime / particleIterator->
				lifeTime;

			particleIterator->color = Math::Lerp(
				particleIterator->startColor, particleIterator->endColor,
				Math::CubicBezier(
					lifeRatio,
					0.2f, 0.0f, 0.0f, 1.0f
				)
			);

			instancingData[numInstance].color = particleIterator->color;
			// instancingData[numInstance].color.w = 1.0f - lifeRatio;

			// スケールを時間に応じて変化させる（startSizeからendSizeへ補間）
			particleIterator->transform.scale = Math::Lerp(
				particleIterator->startSize, particleIterator->endSize,
				lifeRatio);

			Mat4 worldMat;

			// ビルボード
			{
				// カメラの位置と上方向を取得
				Vec3 camPos = camera_->GetViewMat().Inverse().GetTranslate();
				Vec3 up     = Vec3::up;

				Vec3 particlePos = particleIterator->transform.translate;

				// Z軸（カメラ→パーティクル方向、正規化）
				Vec3 zaxis = (particlePos - camPos).Normalized();
				// X軸（upとzaxisの外積、正規化）
				Vec3 xaxis = up.Cross(zaxis).Normalized();
				// Y軸（zaxisとxaxisの外積、正規化）
				Vec3 yaxis = zaxis.Cross(xaxis);

				// 4x4行列を作成
				Mat4 billboardMat    = Mat4::identity;
				billboardMat.m[0][0] = xaxis.x;
				billboardMat.m[0][1] = xaxis.y;
				billboardMat.m[0][2] = xaxis.z;
				billboardMat.m[1][0] = yaxis.x;
				billboardMat.m[1][1] = yaxis.y;
				billboardMat.m[1][2] = yaxis.z;
				billboardMat.m[2][0] = zaxis.x;
				billboardMat.m[2][1] = zaxis.y;
				billboardMat.m[2][2] = zaxis.z;

				// 現在の回転角度を計算（初期回転 + 時間経過による回転）
				float currentRotation = particleIterator->initialRotation;
				// +
				// 	particleIterator->rotationSpeed * particleIterator->
				// 	currentTime;

				// ビルボード面での回転行列を作成
				Mat4 rotationMat = Mat4::RotateZ(currentRotation);

				// ビルボード行列に回転を適用
				worldMat = Mat4::Scale(particleIterator->transform.scale)
					* rotationMat // 回転を適用
					* billboardMat
					* Mat4::Translate(particleIterator->transform.translate);
			}


			Mat4 worldViewProjMat;

			// カメラが存在する場合はカメラから行列を持ってくる
			const Mat4& viewProjMat = CameraManager::GetActiveCamera()->
				GetViewProjMat();
			worldViewProjMat = worldMat * viewProjMat;

			instancingData[numInstance].wvp   = worldViewProjMat;
			instancingData[numInstance].world = worldMat;

			++numInstance;
		}
		++particleIterator; // 次のイテレータに進める
	}

	emitter_.frequencyTime += deltaTime; // 時刻を進める
	// 頻度より大きいなら発生
	if (emitter_.frequency <= emitter_.frequencyTime) {
		particles_.splice(particles_.end(),
		                  Emit(emitter_, shapeType, coneAngle, drag, gravity,
		                       Vec3::zero, Vec4::white, Vec4::white, Vec3::one,
		                       Vec3::one));           // 発生処理
		emitter_.frequencyTime -= emitter_.frequency; // 余計に過ぎた時間も加味して頻度計算する
	}

	// エミッターの形状を描画
	switch (shapeType) {
	case 0: // Sphere
		Debug::DrawSphere(
			emitter_.transform.translate, Quaternion::identity, emitter_.size.x,
			{1.0f, 0.0f, 0.0f, 1.0f}
		);
		break;
	case 1: // Cube
		Debug::DrawBox(emitter_.transform.translate, Quaternion::identity,
		               emitter_.size, {0.0f, 1.0f, 0.0f, 1.0f});
		break;
	default: break;
	}
}

void ParticleObject::Draw() const {
	// 頂点バッファの設定
	D3D12_VERTEX_BUFFER_VIEW vbView = particleCommon_->GetVertexBuffer()->
		View();
	particleCommon_->GetD3D12()->GetCommandList()->IASetVertexBuffers(
		0, 1, &vbView);

	// 定数バッファの設定
	particleCommon_->GetD3D12()->GetCommandList()->
	                 SetGraphicsRootConstantBufferView(
		                 0, materialResource_->GetAddress()
	                 );

	// SRVを設定
	particleCommon_->GetD3D12()->GetCommandList()->
	                 SetGraphicsRootDescriptorTable(
		                 1, srvManager_->GetGPUDescriptorHandle(srvIndex_)
	                 );

	// SRVのDescriptorTableの先頭を設定
	particleCommon_->GetD3D12()->GetCommandList()->
	                 SetGraphicsRootDescriptorTable(
		                 2, TexManager::GetInstance()->GetSrvHandleGPU(
			                 textureFilePath_)
	                 );

	// インデックスバッファの設定
	D3D12_INDEX_BUFFER_VIEW indexBufferView = indexBuffer_->View();
	particleCommon_->GetD3D12()->GetCommandList()->IASetIndexBuffer(
		&indexBufferView);
	// 描画
	particleCommon_->GetD3D12()->GetCommandList()->DrawIndexedInstanced(
		static_cast<UINT>(particleCommon_->GetVertices().size()), numInstance,
		0, 0, 0
	);

	/*particleCommon_->GetD3D12()->GetCommandList()->DrawInstanced(
		static_cast<UINT>(particleCommon_->GetVertices().size()), numInstance, 0, 0
	);*/
}

void ParticleObject::Shutdown() {
	if (materialResource_) {
		materialResource_.reset();
	}
	if (instancingResource_) {
		instancingResource_.reset();
	}
	if (indexBuffer_) {
		indexBuffer_.reset();
	}
}

Particle ParticleObject::MakeNewParticle(const Vec3& pos, const Vec3& vel,
                                         const Vec3& drag,
                                         const Vec3& gravity,
                                         const Vec4  startColor,
                                         const Vec4  endColor,
                                         const Vec3& startSize,
                                         const Vec3& endSize
) {
	Particle particle;
	particle.transform.scale = {0.25f, 0.25f, 0.25f};
	particle.transform.rotate = Vec3::zero;
	const Vec3 rand = Random::Vec3Range(-Vec3::one * 0.1f, Vec3::one * 0.1f);
	particle.transform.translate = pos + rand;

	particle.vel = vel;

	particle.drag    = drag;
	particle.gravity = gravity;

	particle.color = Vec4::white;

	particle.startColor = startColor;
	particle.endColor   = endColor;

	particle.startSize = startSize;
	particle.endSize   = endSize;

	// ランダムな初期回転を設定
	particle.initialRotation = Random::FloatRange(0.0f, 0.1f);
	particle.rotationSpeed   = 0.0f;
	// 今回は回転させない // Random::FloatRange(-Math::pi, Math::pi);


	// 生存時間
	particle.lifeTime    = Random::FloatRange(0.1f, 1.0f);
	particle.currentTime = 0.0f;

	return particle;
}

std::list<Particle> ParticleObject::Emit(
	const Emitter& emitter, int shapeType, [[maybe_unused]] float coneAngle,
	[[maybe_unused]] const Vec3& drag, const Vec3& gravity,
	const Vec3& velocity,
	Vec4 startColor, Vec4 endColor,
	const Vec3& startSize, const Vec3& endSize
) {
	std::list<Particle> particles;
	for (uint32_t count = 0; count < emitter.count; ++count) {
		Vec3 position =
			GeneratePosition(emitter.transform.translate, shapeType);
		//Vec3 velocity = GenerateConeVelocity(coneAngle);
		Particle particle = MakeNewParticle(position, velocity, Vec3::zero,
		                                    gravity, startColor, endColor,
		                                    startSize, endSize
		);
		particles.push_back(particle);
	}
	return particles;
}

void ParticleObject::SetCamera(CameraComponent* newCamera) {
	camera_ = newCamera;
}

Vec3 ParticleObject::GeneratePosition(const Vec3& emitterPosition,
                                      int         shapeType) const {
	switch (shapeType) {
	case 0: // Sphere（球）
	{
		// ランダムな方向
		Vec3 direction = Random::Vec3Range(-Vec3::one * 0.01f,
		                                   Vec3::one * 0.01f);
		direction.Normalize();
		// ランダムな半径
		float radius = Random::FloatRange(0.0f, 0.01f); // サイズを考慮
		return emitterPosition + direction * radius;
	}
	case 1: // Cube（立方体）
	{
		// -1.0fから1.0fの範囲でランダムな位置
		Vec3 offset = Random::Vec3Range(-Vec3::one, Vec3::one);
		return emitterPosition + offset * emitter_.size; // サイズを考慮
	}
	default: return emitterPosition;
	}
}

Vec3 ParticleObject::GenerateConeVelocity(float coneAngle) {
	// コーンの中心方向（例としてY軸正方向）
	Vec3 coneDirection = Vec3::up;

	// コーンの角度をラジアンに変換
	float angleRad = coneAngle * Math::deg2Rad;

	// ランダムな円周上の角度を生成
	float theta = Random::FloatRange(0.0f, 2.0f * std::numbers::pi_v<float>);
	// コーン内のランダムな角度を生成
	float cosPhi = Random::FloatRange(cos(angleRad), 1.0f);
	float sinPhi = sqrt(1.0f - cosPhi * cosPhi);

	// ディレクションベクトルを計算
	Vec3 randomDir;
	randomDir.x = sinPhi * cos(theta);
	randomDir.y = sinPhi * sin(theta);
	randomDir.z = cosPhi;

	// コーンの方向がVec3::upでない場合、回転を適用
	if (!(coneDirection - Vec3::up).IsZero()) {
		// 回転軸を計算
		Vec3 rotationAxis = Vec3::up.Cross(coneDirection);
		rotationAxis.Normalize();
		// 回転角を計算
		float angleBetween = acos(Vec3::up.Dot(coneDirection));
		// クォータニオンを生成
		Quaternion rotation(rotationAxis, angleBetween);
		// 回転を適用
		randomDir = rotation * randomDir;
	}

	// 速度の大きさを設定（例として1.0f）
	float speed = 1.0f;
	return randomDir * speed;
}

void ParticleObject::EmitParticlesAtPosition(const Vec3& position,
                                             int shapeType, float coneAngle,
                                             const Vec3& drag,
                                             const Vec3& gravity,
                                             const Vec3& velocity,
                                             uint32_t count,
                                             const Vec4 startColor,
                                             const Vec4 endColor,
                                             const Vec3& startSize,
                                             const Vec3& endSize
) {
	Emitter localEmitter             = emitter_;
	localEmitter.transform.translate = position;
	localEmitter.count               = count;
	localEmitter.size                = startSize; // エミッターのサイズを初期化
	particles_.splice(particles_.end(),
	                  Emit(localEmitter, shapeType, coneAngle, drag, gravity,
	                       velocity, startColor, endColor,
	                       startSize, endSize));
}
