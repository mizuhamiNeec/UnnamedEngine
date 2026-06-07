#include "SceneConstants.hlsli"

Texture2D    gBaseColorTex : register(t0);
Texture2D    gShadowMap : register(t1);
SamplerState gLinearWrap : register(s0);

struct VsIn {
	float3 pos : POSITION;
	float3 nrm : NORMAL;
	float2 uv : TEXCOORD0;
	float4 boneIndices : TEXCOORD1;
	float4 boneWeights : TEXCOORD2;
};

struct VsOut {
	float4 pos : SV_POSITION;
	float3 normalWS : TEXCOORD0;
	float2 uv : TEXCOORD1;
	float3 positionWS : TEXCOORD2;
};

VsOut VsMain(VsIn i) {
	VsOut  o;
	float3 localPos = i.pos;
	float3 localNrm = i.nrm;

	if (gSkinningInfo.y > 0.5f) {
		float4 skinnedPos = 0.0f;
		float3 skinnedNrm = 0.0f;

		[unroll]
		for (uint k = 0; k < 4; ++k) {
			const float weight = i.boneWeights[k];
			if (weight <= 0.0f) {
				continue;
			}

			const uint boneIndex = min((uint)i.boneIndices[k], 511u);
			skinnedPos           += mul(
				float4(i.pos, 1.0f), gSkinMatrices[boneIndex]
			) * weight;
			skinnedNrm += mul(
				float4(i.nrm, 0.0f), gSkinMatrices[boneIndex]
			).xyz * weight;
		}

		localPos = skinnedPos.xyz;
		localNrm = normalize(skinnedNrm);
	}

	float4 wp = mul(float4(localPos, 1.0f), gWorld);
	o.pos = mul(wp, gViewProj);
	o.positionWS = wp.xyz;
	o.normalWS = normalize(mul(float4(localNrm, 0.0f), gWorldInvTranspose).xyz);
	o.uv = i.uv;
	return o;
}

/// @brief Reverse-Z ShadowMap の 1 sample compare。GREATER_EQUAL depth pass と合わせる。
float SampleDirectionalShadowTexel(int2 texel, uint2 shadowSize, float currentDepth) {
	if (
		texel.x < 0 || texel.y < 0 ||
		texel.x >= (int)shadowSize.x || texel.y >= (int)shadowSize.y
	) {
		return 1.0f;
	}

	const float storedDepth = gShadowMap.Load(int3(texel, 0)).r;
	return (currentDepth + gShadowParams.x >= storedDepth) ? 1.0f : 0.0f;
}

/// @brief Directional ShadowMap を評価する。PCF 無効時は 1 tap compare。
float ComputeDirectionalShadowVisibility(float3 positionWS, float3 normalWS) {
	if (gShadowParams.w <= 0.5f) {
		return 1.0f;
	}

	float3 biasedPositionWS = positionWS + normalize(normalWS) * gShadowFilterParams.z;
	float4 lightClip = mul(float4(biasedPositionWS, 1.0f), gShadowLightViewProj);
	if (lightClip.w <= 0.0f) {
		return 1.0f;
	}

	float3 lightNdc = lightClip.xyz / lightClip.w;
	float2 shadowUv = float2(lightNdc.x * 0.5f + 0.5f, 0.5f - lightNdc.y * 0.5f);
	if (
		shadowUv.x < 0.0f || shadowUv.x > 1.0f ||
		shadowUv.y < 0.0f || shadowUv.y > 1.0f ||
		lightNdc.z < 0.0f || lightNdc.z > 1.0f
	) {
		return 1.0f;
	}

	uint shadowWidth = 1;
	uint shadowHeight = 1;
	gShadowMap.GetDimensions(shadowWidth, shadowHeight);
	const uint2 shadowSize = uint2(shadowWidth, shadowHeight);
	const int2 shadowTexel = int2(min(
		uint2(shadowUv * float2(shadowSize)),
		shadowSize - 1u
	));
	const float currentDepth = lightNdc.z;
	if (gShadowFilterParams.x <= 0.5f) {
		return SampleDirectionalShadowTexel(
			shadowTexel, shadowSize, currentDepth
		);
	}

	const int radius = max(0, (int)round(gShadowFilterParams.y));
	float visibility = 0.0f;
	[unroll]
	for (int y = -1; y <= 1; ++y) {
		[unroll]
		for (int x = -1; x <= 1; ++x) {
			visibility += SampleDirectionalShadowTexel(
				shadowTexel + int2(x, y) * radius,
				shadowSize,
				currentDepth
			);
		}
	}
	return visibility / 9.0f;
}

struct MaterialEvalInput {
	float3 albedo;
	float3 emissive;
	float3 normalWS;
	float3 viewDirWS;
	float3 lightDirWS;
	float3 halfDirWS;
	float3 positionWS;
	float3 lightColor;
	float  shadowFactor;
};

float3 EvaluateUnlit(MaterialEvalInput input) {
	return input.albedo + input.emissive;
}

static const float kPi = 3.14159265359f;

float DistributionGGX(float ndh, float roughness) {
	const float a  = roughness * roughness;
	const float a2 = a * a;
	const float d  = ndh * ndh * (a2 - 1.0f) + 1.0f;
	return a2 / max(kPi * d * d, 0.000001f);
}

float GeometrySchlickGGX(float ndv, float roughness) {
	const float r = roughness + 1.0f;
	const float k = (r * r) / 8.0f;
	return ndv / max(ndv * (1.0f - k) + k, 0.000001f);
}

float GeometrySmith(float ndv, float ndl, float roughness) {
	return GeometrySchlickGGX(ndv, roughness) *
		GeometrySchlickGGX(ndl, roughness);
}

float3 FresnelSchlick(float cosTheta, float3 f0) {
	return f0 + (1.0f - f0) * pow(1.0f - saturate(cosTheta), 5.0f);
}

float3 EvaluateLitPBR(MaterialEvalInput input) {
	const float metallic  = saturate(gMetallic);
	const float roughness = max(saturate(gRoughness), 0.04f);

	const float ndl = saturate(dot(input.normalWS, input.lightDirWS));
	const float ndv = saturate(dot(input.normalWS, input.viewDirWS));
	const float ndh = saturate(dot(input.normalWS, input.halfDirWS));
	const float vdh = saturate(dot(input.viewDirWS, input.halfDirWS));

	const float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), input.albedo, metallic);
	const float3 f  = FresnelSchlick(vdh, f0);
	const float  d  = DistributionGGX(ndh, roughness);
	const float  g  = GeometrySmith(ndv, ndl, roughness);

	const float3 specular = (d * g * f) / max(4.0f * ndv * ndl, 0.0001f);
	const float3 kd       = (1.0f - f) * (1.0f - metallic);
	const float3 diffuse  = kd * input.albedo / kPi;
	const float3 radiance = input.lightColor * ndl * input.shadowFactor;
	return (diffuse + specular) * radiance + input.emissive;
}

float4 PsMain(VsOut i) : SV_Target {
	float4 baseColor = gBaseColorTex.Sample(gLinearWrap, i.uv) * gBaseColor;

	MaterialEvalInput input;
	input.albedo      = baseColor.rgb;
	input.emissive    = gEmissiveColor.rgb;
	input.normalWS    = normalize(i.normalWS);
	input.viewDirWS   = normalize(gCameraPos - i.positionWS);
	input.lightDirWS  = normalize(gDirectionToLight.xyz);
	input.halfDirWS   = normalize(input.viewDirWS + input.lightDirWS);
	input.positionWS  = i.positionWS;
	input.lightColor  =
		gDirectionalLightColorIntensity.rgb * gDirectionalLightColorIntensity.a;

	float shadowVisibility = ComputeDirectionalShadowVisibility(
		i.positionWS, input.normalWS
	);
	input.shadowFactor = lerp(
		1.0f, shadowVisibility, saturate(gShadowParams.y)
	);

	float3 lit = (gDomainMode < 0.5f || gShadingModel > 1.5f)
		? EvaluateUnlit(input)
		: EvaluateLitPBR(input);
	return float4(lit, saturate(gOpacity * baseColor.a));
}
