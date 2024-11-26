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

struct Camera {
	float3 worldPosition;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
Texture2D<float4> gTexture : register(t0);
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
		// 入射光の拡散反射計算
		float cosTheta = saturate(dot(input.normal, -gDirectionalLight.direction));

		// 視線方向と反射ベクトルの計算
		float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
		float3 reflectLight = reflect(gDirectionalLight.direction, input.normal);

		float3 halfVector = normalize(-gDirectionalLight.direction + toEye);
		float NDotH = dot(normalize(input.normal), halfVector);
		
		// ただのPhong
		//float RdotE = saturate(dot(reflectLight, toEye));
		float specularPow = pow(NDotH, gMaterial.shininess);

		// 拡散反射
		float3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cosTheta *
			gDirectionalLight.intensity;

		// 鏡面反射
		float3 specular = gMaterial.specularColor * gDirectionalLight.color.rgb * gDirectionalLight.intensity *
			specularPow;

		// 合成色
		output.color.rgb = diffuse + specular;
		output.color.a = gMaterial.color.a * textureColor.a;
	}
	else {
		// ライティングを無効にする場合
		output.color = gMaterial.color * textureColor;
	}

	// sRGB
	// output.color.rgb = pow(output.color.rgb, 1 / 2.2);

	return output;
}
