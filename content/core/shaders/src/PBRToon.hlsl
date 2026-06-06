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

/// @brief 頂点をワールド空間へ変換し、必要に応じてスキニングを適用する。
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

/// @brief Directional ShadowMap を評価する。PCF 無効時は従来の 1 tap compare。
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

/// @brief PBR入力を活かしつつトゥーン調ライティングを適用して最終色を出力する。
float4 PsMain(VsOut i) : SV_Target {
	// ベースカラーをマテリアル係数込みで計算する。
	float4 baseColor = gBaseColorTex.Sample(gLinearWrap, i.uv) * gBaseColor;
	float3 albedo    = baseColor.rgb;

	// Unlit ドメインは従来どおりライティングを行わない。
	if (gDomainMode < 0.5f) {
		float3 unlit = albedo + gEmissiveColor.rgb;
		return float4(unlit, saturate(gOpacity * baseColor.a));
	}

	float3 N = normalize(i.normalWS);
	float3 V = normalize(gCameraPos - i.positionWS);
	float3 L = normalize(gDirectionToLight.xyz);
	float3 H = normalize(V + L);

	float ndl = saturate(dot(N, L));
	float diffuseFactor;
	if (ndl > 0.75f) {
		diffuseFactor = 1.00f;
	} else if (ndl > 0.40f) {
		diffuseFactor = 0.70f;
	} else {
		diffuseFactor = 0.40f;
	}

	// ラフネスからハイライト幅を決め、トゥーン帯に量子化する。
	float  specPow   = lerp(96.0f, 8.0f, saturate(gRoughness));
	float  spec      = pow(saturate(dot(N, H)), specPow);
	float  specBand  = spec;
	float3 specColor = lerp(
		float3(0.04f, 0.04f, 0.04f), albedo, saturate(gMetallic)
	);
	float3 specularToon = specColor * specBand * 0.25f;

	// 輪郭側を軽く持ち上げて読みやすさを確保する。
	float  rimDot       = 1.0f - saturate(dot(N, V));
	float  rimIntensity = pow(rimDot, 3.0f);
	float3 rimColor     = float3(0.025f, 0.025f, 0.025f) * rimIntensity;

	float3 ambient = float3(0.025f, 0.025f, 0.125f) * albedo;
	float shadowVisibility = ComputeDirectionalShadowVisibility(i.positionWS, N);
	float shadowFactor = lerp(
		1.0f,
		shadowVisibility,
		saturate(gShadowParams.y)
	);
	float3 directLit = (albedo * diffuseFactor + specularToon) * shadowFactor;
	float3 lit = directLit + rimColor + ambient + gEmissiveColor.rgb;

	// 側面にわずかな寒色を加えて法線向きを視認しやすくする。
	if (N.y < 0.7f) {
		const float3 synthwaveBlueTint = float3(0.10f, 0.65f, 1.00f);
		float        t                 = saturate(1.0f - N.y / 0.7f);
		lit                            += synthwaveBlueTint * t * 0.01f;
	}

	// 既存トーンに合わせた簡易フォグ。
	float  distanceWS = length(gCameraPos - i.positionWS);
	float  fogStart   = 6.5024f;
	float  fogEnd     = 416.1536f;
	float3 fogColor   = float3(0.78f, 0.22f, 0.92f);
	float  fogFactor  = saturate((distanceWS - fogStart) / (fogEnd - fogStart));
	lit               = lerp(lit, fogColor, fogFactor * 0.5f);

	return float4(lit, saturate(gOpacity * baseColor.a));
}
