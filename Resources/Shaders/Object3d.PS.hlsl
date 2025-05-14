#include "Object3d.hlsli"

struct Material {
	float4 baseColor;
	float  metallic;
	float  roughness;
	float3 emissive;
};

struct DirectionalLight {
	float4 color;
	float3 direction;
	float  intensity;
};

struct Camera {
	float3 worldPosition;
};

ConstantBuffer<Material>         gMaterial : register(b1);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);
ConstantBuffer<Camera>           gCamera : register(b3);
Texture2D                        gBaseColorTexture : register(t0);
TextureCube                      gEnvironmentTexture : register(t1);
//Texture2D brdfLUT : register(t2); // 🌟 BRDF LUT（スクリーンスペース用）
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
	float4 color : SV_TARGET0;
};

// SchlickのFresnel項
float3 FresnelSchlick(float cosTheta, float3 F0) {
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// GGX 法線分布関数（NDF）
float NormalDistribution_GGX(float NdotH, float roughness) {
	float a      = roughness * roughness;
	float a2     = a * a;
	float NdotH2 = NdotH * NdotH;
	float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
	return a2 / (3.141592 * denom * denom);
}

// Smith-Schlickの幾何減衰項（G）
float Geometry_SmithGGX(float NdotV, float NdotL, float roughness) {
	float r   = roughness + 1.0;
	float k   = (r * r) / 8.0;
	float G_V = NdotV / (NdotV * (1.0 - k) + k);
	float G_L = NdotL / (NdotL * (1.0 - k) + k);
	return G_V * G_L;
}

PixelShaderOutput PSMain(VertexShaderOutput input) {
	PixelShaderOutput output;

	// テクスチャサンプリング
	float4 baseColor = gBaseColorTexture.Sample(gSampler, input.texcoord) *
		gMaterial.baseColor;

	float  metallic  = gMaterial.metallic;
	float  roughness = max(gMaterial.roughness, 0.05);
	float3 emissive  = gMaterial.emissive;

	float3 N = normalize(input.normal);
	float3 V = normalize(gCamera.worldPosition - input.worldPosition);
	float3 R = reflect(-V, N);

	float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColor.rgb, metallic);

	static const float environmentIntensity = 4.0f;

	// IBL
	float3 irradiance = gEnvironmentTexture.Sample(gSampler, N).rgb *
		environmentIntensity;
	float3 prefiltered = gEnvironmentTexture.Sample(gSampler, R).rgb *
		environmentIntensity;

	float3 diffuseIBL  = irradiance * baseColor.rgb * (1.0 - metallic);
	float3 specularIBL = prefiltered * F0;

	// Directional Light
	float3 L = normalize(-gDirectionalLight.direction);
	float3 H = normalize(V + L);

	float NdotL = saturate(dot(N, L));
	float NdotV = saturate(dot(N, V));
	float NdotH = saturate(dot(N, H));
	float HdotV = saturate(dot(H, V));

	float3 F = FresnelSchlick(HdotV, F0);
	float  D = NormalDistribution_GGX(NdotH, roughness);
	float  G = Geometry_SmithGGX(NdotV, NdotL, roughness);

	float3 specular = (D * F * G) / max(4.0 * NdotV * NdotL, 0.001);
	float3 diffuse  = (1.0 - F) * baseColor.rgb * (1.0 - metallic) / 3.141592;

	float3 lightColor = gDirectionalLight.color.rgb * gDirectionalLight.
		intensity;
	float3 radiance = lightColor * NdotL;

	float3 directLight = (diffuse + specular) * radiance;

	// 出力
	output.color.rgb = diffuseIBL + specularIBL + directLight + emissive;
	output.color.a   = baseColor.a;

	return output;
}
