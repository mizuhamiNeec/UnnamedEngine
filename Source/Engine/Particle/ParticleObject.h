#pragma once

#include <list>
#include <memory>

#include "../Lib/Math/Vector/Vec3.h"
#include "../Lib/Structs/Structs.h"

#include "../Renderer/ConstantBuffer.h"
#include "../Renderer/IndexBuffer.h"
#include "../Renderer/VertexBuffer.h"

class Camera;
class SrvManager;
class ParticleCommon;

class ParticleObject {
public:
	~ParticleObject() = default;

	void Init(ParticleCommon* particleCommon, const std::string& textureFilePath);
	void Update(float deltaTime);
	void Draw() const;

	static Particle MakeNewParticle(const Vec3& pos, const Vec3& vel, const Vec3& drag, const Vec3& gravity);

	std::list<Particle> Emit(const Emitter& emitter, int shapeType, float coneAngle, const Vec3& drag, const Vec3& gravity);

	void SetCamera(Camera* newCamera);
	Vec3 GeneratePosition(const Vec3& emitterPosition, int shapeType);
	static Vec3 GenerateConeVelocity(float coneAngle);

private:
	ParticleCommon* particleCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Camera* camera_ = nullptr;
	std::string textureFilePath_;

	uint32_t kNumMaxInstance = 512;
	uint32_t numInstance = 0; // 描画すべきインスタンス数

	uint32_t srvIndex_ = 0;

	// パーティクル
	std::list<Particle> particles_;

	Emitter emitter_ = {};
	AccelerationField accelerationField_ = {};

	std::vector<Vertex> vertices_;
	std::unique_ptr<VertexBuffer<Vertex>> vertexBuffer_ = nullptr;
	std::unique_ptr<ConstantBuffer> materialResource_ = nullptr;
	std::unique_ptr<ConstantBuffer> instancingResource_ = nullptr;
	std::unique_ptr<IndexBuffer> indexBuffer_ = nullptr;
	// バッファリソース内のデータを指すポインタ
	Vertex* vertexData_ = nullptr;
	uint32_t* indexData_ = nullptr;
	Material* materialData_ = nullptr;
	ParticleForGPU* instancingData = nullptr;

	bool enableGravity = true;
	bool enableAccelerationField_ = true;
	bool enableDrag = true;
};
