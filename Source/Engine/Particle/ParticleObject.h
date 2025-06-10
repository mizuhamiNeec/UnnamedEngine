#pragma once

#include <list>
#include <memory>

#include <Components/Camera/CameraComponent.h>

#include <Lib/Math/Vector/Vec3.h>
#include <Lib/Structs/Structs.h>

#include <Renderer/ConstantBuffer.h>
#include <Renderer/IndexBuffer.h>
#include <Renderer/VertexBuffer.h>

#include "SrvManager.h"

class Camera;
class ParticleManager;

enum class BillboardType {
	None, // ビルボードなし
	XY,   // XY平面にビルボード
	XZ,   // XZ平面にビルボード
	YZ,   // YZ平面にビルボード
	All   // 全方向にビルボード
};

class ParticleObject {
public:
	~ParticleObject() = default;

	void Init(ParticleManager*   particleCommon,
	          const std::string& textureFilePath);
	void Update(float deltaTime);
	void Draw() const;

	void Shutdown();

	static Particle MakeNewParticle(const Vec3& pos, const Vec3& vel,
	                                const Vec3& drag, const Vec3& gravity, Vec4 startColor, Vec4 endColor, const
	                                Vec3& startSize, const Vec3& endSize);

	std::list<Particle> Emit(
		const Emitter& emitter, int shapeType, float coneAngle,
		const Vec3& drag, const Vec3& gravity, const Vec3& velocity, Vec4 startColor, Vec4
		endColor, const Vec3& startSize, const Vec3& endSize
	);

	void SetCamera(CameraComponent* newCamera);
	Vec3 GeneratePosition(const Vec3& emitterPosition, int shapeType) const;
	static Vec3 GenerateConeVelocity(float coneAngle);

	void EmitParticlesAtPosition(const Vec3& position, int shapeType,
	                             float coneAngle, const Vec3& drag,
	                             const Vec3& gravity, const Vec3& velocity,
	                             uint32_t count, Vec4 startColor, Vec4 endColor, const Vec3& startSize, const
	                             Vec3& endSize);

	void SetBillboardType(const BillboardType type) {
		billboardType_ = type;
	}

	[[nodiscard]] BillboardType GetBillboardType() const {
		return billboardType_;
	}

private:
	struct Material {
		Vec4    color;
		int32_t enableLighting;
		Vec3    padding;
		Mat4    uvTransform;
		float   shininess;
		Vec3    specularColor;
	};

	BillboardType billboardType_ = BillboardType::All;

	ParticleManager* particleCommon_ = nullptr;
	SrvManager*      srvManager_     = nullptr;
	CameraComponent* camera_         = nullptr;
	std::string      textureFilePath_;

	uint32_t kNumMaxInstance = 16385; // 最大インスタンス数
	uint32_t numInstance     = 0;     // 描画すべきインスタンス数

	uint32_t srvIndex_ = 0;

	// パーティクル
	std::list<Particle> particles_;

	Emitter           emitter_           = {};
	AccelerationField accelerationField_ = {};

	std::unique_ptr<ConstantBuffer> materialResource_   = nullptr;
	std::unique_ptr<ConstantBuffer> instancingResource_ = nullptr;
	std::unique_ptr<IndexBuffer>    indexBuffer_        = nullptr;
	// バッファリソース内のデータを指すポインタ
	Vertex*         vertexData_    = nullptr;
	uint32_t*       indexData_     = nullptr;
	Material*       materialData_  = nullptr;
	ParticleForGPU* instancingData = nullptr;

	bool enableGravity            = true;
	bool enableAccelerationField_ = true;
	bool enableDrag               = true;
};
