#include "Object3d.hlsli"

struct Material {
	float4 color;
	int enableLighting;
	float4x4 uvTransform;
	float shininess;
	float3 specularColor; // 鏡面反射色
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

struct SpotLight {
	float4 color; //!< ライトの色
	float3 position; //!< ライトの位置
	float intensity; //!< 輝度
	float3 direction; //!< スポットライトの方向
	float distance; //!< ライトの届く最大距離
	float decay; //!< 減衰率
	float cosAngle; //!< スポットライトの余弦
	float cosFalloffStart;
};

struct Camera {
	float3 worldPosition; //!< カメラの位置
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<PointLight> gPointLight : register(b3);
ConstantBuffer<SpotLight> gSpotLight : register(b4);
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

	// ライティング
	if (gMaterial.enableLighting != 0)
	{
		float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

		//---------------------------------------------------------------------
		// Purpose: Directional Light
		//---------------------------------------------------------------------
		float3 diffuseDirectionalLight = float3(0.0f, 0.0f, 0.0f);
		float3 specularDirectionalLight = float3(0.0f, 0.0f, 0.0f);
		{
			float cosThetaDir = saturate(dot(input.normal, -gDirectionalLight.direction));
			float3 halfVectorDir = normalize(-gDirectionalLight.direction + toEye);
			float NDotHDir = dot(normalize(input.normal), halfVectorDir);
			float specularPowDir = pow(NDotHDir, gMaterial.shininess);

			diffuseDirectionalLight =
				gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb *
				cosThetaDir * gDirectionalLight.intensity;

			specularDirectionalLight =
				gMaterial.specularColor * gDirectionalLight.color.rgb *
				gDirectionalLight.intensity * specularPowDir;
		}

		//---------------------------------------------------------------------
		// Purpose: Point Light
		//---------------------------------------------------------------------
		float3 diffusePointLight = float3(0.0f, 0.0f, 0.0f);
		float3 specularPointLight = float3(0.0f, 0.0f, 0.0f);
		{
			float3 pointLightDirection = gPointLight.position - input.worldPosition;
			float distance = length(pointLightDirection);
			pointLightDirection = normalize(pointLightDirection);

			float attenuationFactor = pow(saturate(-distance / gPointLight.radius + 1.0), gPointLight.decay);

			float cosThetaPoint = saturate(dot(input.normal, pointLightDirection));
			diffusePointLight =
				gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb *
				cosThetaPoint * gPointLight.intensity * attenuationFactor;

			float3 halfVectorPoint = normalize(pointLightDirection + toEye);
			float NDotHPoint = dot(normalize(input.normal), halfVectorPoint);
			float specularPowPoint = pow(NDotHPoint, gMaterial.shininess);

			specularPointLight =
				gMaterial.specularColor * gPointLight.color.rgb *
				gPointLight.intensity * specularPowPoint * attenuationFactor;
		}

		//---------------------------------------------------------------------
		// Purpose: Spot Light
		//---------------------------------------------------------------------
		float3 diffuseSpotLight = float3(0.0f, 0.0f, 0.0f);
		float3 specularSpotLight = float3(0.0f, 0.0f, 0.0f);
		{
			// スポットライトの位置と方向を計算
			float3 spotLightDirection = gSpotLight.position - input.worldPosition;
			float distance = length(spotLightDirection);
			spotLightDirection = normalize(spotLightDirection);

			// スポットライトの照射角度（余弦値）
			float spotCosTheta = dot(-spotLightDirection, gSpotLight.direction);

			// falloffFactorの計算
			// cosFalloffStart と cosAngle の間で減衰を適用
			float falloffFactor = saturate(
				(spotCosTheta - gSpotLight.cosAngle) / (gSpotLight.cosFalloffStart - gSpotLight.cosAngle)
			);

			// 距離に基づく減衰
			float spotAttenuation = pow(saturate(-distance / gSpotLight.distance + 1.0), gSpotLight.decay);

			// 最終的な減衰
			spotAttenuation *= falloffFactor;

			// 法線との角度を計算
			float cosThetaSpot = saturate(dot(input.normal, spotLightDirection));

			// 拡散反射成分
			diffuseSpotLight =
				gMaterial.color.rgb * textureColor.rgb * gSpotLight.color.rgb *
				cosThetaSpot * gSpotLight.intensity * spotAttenuation;

			// 鏡面反射成分
			float3 halfVectorSpot = normalize(spotLightDirection + toEye);
			float NDotHSpot = dot(normalize(input.normal), halfVectorSpot);
			float specularPowSpot = pow(NDotHSpot, gMaterial.shininess);

			specularSpotLight =
				gMaterial.specularColor * gSpotLight.color.rgb *
				gSpotLight.intensity * specularPowSpot * spotAttenuation;
		}

		// 色の合成
		output.color.rgb =
			diffuseDirectionalLight + specularDirectionalLight +
			diffusePointLight + specularPointLight +
			diffuseSpotLight + specularSpotLight;
		output.color.a = gMaterial.color.a * textureColor.a;
	} else
	{
		// ライティング無効時
		output.color = gMaterial.color * textureColor;
		output.color.a = gMaterial.color.a * textureColor.a;
	}

	// sRGBへの変換（オプション）
	// output.color.rgb = pow(output.color.rgb, 1 / 2.2);

	return output;
}
