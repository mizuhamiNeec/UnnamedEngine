#include "Object3d.hlsli"

struct Material {
	float4 color;
	int enableLighting;
	float4x4 uvTransform;
	float shininess;
	float3 specularColor; // 鏡面反射色を追加
};

struct DirectionalLight {
	float4 color; //!< ライトの色
	float3 direction; //!< ライトの向き
	float intensity; //!< 輝度
};

struct PointLight {
	float4 color; //!< ライトの色
	float3 position; //!< ライトの位置
	float intensity; //!< 輝度
	float radius; //!< ライトの届く最大距離
	float decay; //!< 減衰率
};

struct Camera {
	float3 worldPosition;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<PointLight> gPointLight : register(b3);
Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
	float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
	PixelShaderOutput output;

	// UV変換とテクスチャサンプリング
	float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
	float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

	if (gMaterial.enableLighting != 0) {
		// 指向性ライトの拡散反射計算
		float cosThetaDir = saturate(dot(input.normal, -gDirectionalLight.direction));

		// 視線方向と反射ベクトルの計算（指向性ライト）
		float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

		float3 halfVectorDir = normalize(-gDirectionalLight.direction + toEye);
		float NDotHDir = dot(normalize(input.normal), halfVectorDir);

		float specularPowDir = pow(NDotHDir, gMaterial.shininess);

		// 指向性ライトの拡散反射
		float3 diffuseDirectionalLight =
			gMaterial.color.rgb *
			textureColor.rgb *
			gDirectionalLight.color.rgb *
			cosThetaDir *
			gDirectionalLight.intensity;

		// 指向性ライトの鏡面反射
		float3 specularDirectionalLight =
			gMaterial.specularColor *
			gDirectionalLight.color.rgb *
			gDirectionalLight.intensity *
			specularPowDir;

		// ポイントライトの方向ベクトルと距離の計算
		float3 pointLightDirection = gPointLight.position - input.worldPosition;
		float distance = length(pointLightDirection);
		pointLightDirection = normalize(pointLightDirection);

		// 距離減衰を計算（例として逆二乗則）
		float attenuationFactor = pow(saturate(-distance / gPointLight.radius + 1.0), gPointLight.decay); // 指数によるコントロール

		// ポイントライトの拡散反射計算
		float cosThetaPoint = saturate(dot(input.normal, pointLightDirection));
		float3 diffusePointLight =
			gMaterial.color.rgb *
			textureColor.rgb *
			gPointLight.color.rgb *
			cosThetaPoint *
			gPointLight.intensity *
			attenuationFactor;

		// ポイントライトの鏡面反射計算
		float3 halfVectorPoint = normalize(pointLightDirection + toEye);
		float NDotHPoint = dot(normalize(input.normal), halfVectorPoint);
		float specularPowPoint = pow(NDotHPoint, gMaterial.shininess);

		float3 specularPointLight =
			gMaterial.specularColor *
			gPointLight.color.rgb *
			gPointLight.intensity *
			specularPowPoint *
			attenuationFactor;

		// 合成色
		output.color.rgb =
			diffuseDirectionalLight + specularDirectionalLight +
			diffusePointLight + specularPointLight;

		output.color.a = gMaterial.color.a * textureColor.a;
	}
	else {
		// ライティングを無効にする場合
		output.color = gMaterial.color * textureColor;
	}

	// sRGBへの変換（オプション）
	// output.color.rgb = pow(output.color.rgb, 1 / 2.2);

	return output;
}
