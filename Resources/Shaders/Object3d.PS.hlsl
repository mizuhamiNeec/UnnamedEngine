#include "Object3d.hlsli"

struct Material {
	float4 baseColor;
	float metallic;
	float roughness;
	float3 emissive;
};

struct DirectionalLight {
	float4 color;
	float3 direction;
	float intensity;
};

struct Camera {
	float3 worldPosition;
};

ConstantBuffer<Material> gMaterial : register(b1);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);
ConstantBuffer<Camera> gCamera : register(b3);
Texture2D baseColorTexture : register(t0);
Texture2D envMap : register(t1); // 🌟 IBL用のキューブマップ
//Texture2D brdfLUT : register(t2); // 🌟 BRDF LUT（スクリーンスペース用）
SamplerState samplerState : register(s0);

struct PixelShaderOutput {
	float4 color : SV_TARGET0;
};

// SchlickのFresnel項
float3 FresnelSchlick(float cosTheta, float3 F0) {
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// GGX 法線分布関数（NDF）
float NormalDistribution_GGX(float NdotH, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH2 = NdotH * NdotH;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	return a2 / (3.141592 * denom * denom);
}

// Smith-Schlickの幾何減衰項（G）
float Geometry_SmithGGX(float NdotV, float NdotL, float roughness) {
	float r = roughness + 1.0;
	float k = (r * r) / 8.0;
	float G_V = NdotV / (NdotV * (1.0 - k) + k);
	float G_L = NdotL / (NdotL * (1.0 - k) + k);
	return G_V * G_L;
}

float2 EquirectangularUV(float3 dir) {
	float2 uv;
	uv.x = 0.5 + atan2(dir.z, dir.x) / (2.0 * 3.141592); // 水平方向（360°）
	uv.y = 0.5 - asin(dir.y) / 3.141592; // 垂直方向（180°）
	return uv;
}

PixelShaderOutput PSMain(VertexShaderOutput input) {
	PixelShaderOutput output;

    // テクスチャサンプリング
	float4 baseColor = baseColorTexture.Sample(samplerState, input.texcoord) * gMaterial.baseColor;
    
	float metallic = gMaterial.metallic;
	float roughness = max(gMaterial.roughness, 0.05); // 0.05以下は計算が不安定なのでクランプ
	float3 emissive = gMaterial.emissive;

	float3 N = normalize(input.normal);
	float3 V = normalize(gCamera.worldPosition - input.worldPosition);
	float3 R = reflect(-V, N);

    // 金属度に応じてF0を計算（非金属は0.04, 金属はbaseColor）
	float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColor.rgb, metallic);

    // ----------------------------------------------------------------------------
    // **IBLによる間接光の追加**
    // ----------------------------------------------------------------------------
	float2 uvDiffuse = EquirectangularUV(N);
	float3 irradiance = envMap.Sample(samplerState, uvDiffuse).rgb; // ☀ 拡散IBL
	float2 uvSpecular = EquirectangularUV(R);
	float3 prefiltered = envMap.Sample(samplerState, uvSpecular).rgb; // ✨ 鏡面IBL
	//float2 brdf = brdfLUT.Sample(samplerState, float2(dot(N, V), roughness)).rg;

	float3 diffuseIBL = irradiance * baseColor.rgb * (1.0 - metallic);
	float3 specularIBL = prefiltered * (F0 /* * brdf.x + brdf.y*/);

    // ----------------------------------------------------------------------------
    // Directional Light（方向光）
    // ----------------------------------------------------------------------------
	float3 L = normalize(-gDirectionalLight.direction);
	float3 H = normalize(V + L);

	float NdotL = saturate(dot(N, L));
	float NdotV = saturate(dot(N, V));
	float NdotH = saturate(dot(N, H));
	float HdotV = saturate(dot(H, V));

    // BRDFの計算
	float3 F = FresnelSchlick(HdotV, F0);
	float D = NormalDistribution_GGX(NdotH, roughness);
	float G = Geometry_SmithGGX(NdotV, NdotL, roughness);

	float3 specular = (D * F * G) / max(4.0 * NdotV * NdotL, 0.001);
	float3 diffuse = (1.0 - F) * baseColor.rgb * (1.0 - metallic) / 3.141592;

	float3 lightColor = gDirectionalLight.color.rgb * gDirectionalLight.intensity;
	float3 radiance = lightColor * NdotL;

	float3 directLight = (diffuse + specular) * radiance;

    // ----------------------------------------------------------------------------
    // 出力
    // ----------------------------------------------------------------------------
	output.color.rgb = diffuseIBL + specularIBL + directLight + emissive;
	output.color.a = baseColor.a; // アルファは元の値を維持

	return output;
}