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

// Schlick-IBL向けのFresnel式
float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness) {
	return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness),
	                 F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

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

#if 0 // 元のPBRシェーダーを無効化
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

	float  NdotV = saturate(dot(N, V));
	float3 F0    = lerp(float3(0.04, 0.04, 0.04), baseColor.rgb, metallic);

	// 環境光の強度を上げて影を明るく
	static const float environmentIntensity = 2.0f;
	// アンビエントライトを追加（影の部分を明るくする）
	static const float3 ambientLight = float3(0.15, 0.15, 0.18); // 薄い青みがかった環境光

	// IBL
	float3 irradiance = gEnvironmentTexture.Sample(gSampler, N).rgb *
		environmentIntensity;

	const float MAX_REFLECTION_LOD = 4.0f;
	float mipLevel = roughness * MAX_REFLECTION_LOD;
	float3 prefiltered = gEnvironmentTexture.SampleLevel(gSampler, R, mipLevel).
	                                         rgb *
		environmentIntensity;

	// IBLのFresnel項は視線方向と法線を使用
	float3 F_IBL = FresnelSchlickRoughness(NdotV, F0, roughness);

	// IBLの拡散反射と鏡面反射の調整
	float3 diffuseIBL  = irradiance * baseColor.rgb * (1.0 - metallic);
	float3 specularIBL = prefiltered * F_IBL;

	// 反射を強化（特に金属表面で）
	float reflectionStrength = lerp(0.1, 1.0, metallic); // 金属度に応じた反射強度
	reflectionStrength *= (1.0 - roughness * 0.8);       // ラフネスで反射を減衰

	// 環境マップからの直接反射（よりクリアな反射）
	float3 environmentReflection = gEnvironmentTexture.SampleLevel(
		gSampler, R, 0).rgb;
	float3 clearReflection = environmentReflection * F_IBL * reflectionStrength
		* environmentIntensity;

	// ラフネスが高いほどspecularIBLを弱める（元のコード）
	float specularFactor = (1.0 - roughness) * (0.5 + 0.5 * metallic);
	specularIBL *= specularFactor;

	// Directional Light
	float3 L = normalize(-gDirectionalLight.direction);
	float3 H = normalize(V + L);

	float NdotL = saturate(dot(N, L));
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

	// アンビエントライトを追加して影の部分を明るく
	float3 ambientContribution = ambientLight * baseColor.rgb * (1.0 - metallic
		* 0.5);

	// 出力（アンビエントライト + クリアな環境反射を追加）
	output.color.rgb = diffuseIBL + specularIBL + directLight +
		ambientContribution + clearReflection + emissive;
	output.color.a = baseColor.a;

	return output;
}
#endif

PixelShaderOutput PSMain(VertexShaderOutput input) {
	PixelShaderOutput output;

	// 基本的な値の準備
	float4 baseColor = gBaseColorTexture.Sample(gSampler, input.texcoord) *
		gMaterial.baseColor;
	float3 N          = normalize(input.normal);
	float3 V          = normalize(gCamera.worldPosition - input.worldPosition);
	float3 L          = normalize(-gDirectionalLight.direction);
	float3 lightColor = gDirectionalLight.color.rgb * gDirectionalLight.
		intensity;

	// --- Toon Shading 風のライティング ---
	float NdotL = saturate(dot(N, L));

	// 3段階のトゥーンシェーディング
	float diffuseFactor;
	if (NdotL > 0.75) {
		diffuseFactor = 1.0;
	} else if (NdotL > 0.4) {
		diffuseFactor = 0.7;
	} else {
		diffuseFactor = 0.4;
	}

	float3 diffuse = baseColor.rgb * lightColor * diffuseFactor;

	// --- リムライト ---
	// 輪郭を強調して奥行き感を出す
	float  rimDot       = 1.0 - saturate(dot(N, V));
	float  rimIntensity = 1.0 - pow(rimDot, 3.0) * 1.25;        // 輪郭を強く光らせる
	float3 rimColor     = float3(0.025, 0.025, 0.025) * rimIntensity; // 少し黄色がかったリムライト

	// --- 環境光 ---
	// 全体的な明るさを確保するための環境光
	float3 ambient = float3(0.025, 0.025, 0.125) * baseColor.rgb;

	// --- 合成 ---
	// ディフューズ光 + リムライト + 環境光 + 自己発光
	float3 finalColor = diffuse + rimColor + ambient + gMaterial.emissive;

    // --- フォグ ---
    // 奥に行くほど青みがかって暗くなるように
    float  distance    = length(gCamera.worldPosition - input.worldPosition);
    float  fogStart    = 0.0;
    float  fogEnd      = 150.0;
    float3 fogColor    = float3(0.1, 0.15, 0.25); // 青みがかった暗い色
    float  fogFactor   = saturate((distance - fogStart) / (fogEnd - fogStart));
    
    finalColor = lerp(finalColor, fogColor, fogFactor);

	output.color.rgb = finalColor;
	output.color.a   = baseColor.a;

	return output;
}
