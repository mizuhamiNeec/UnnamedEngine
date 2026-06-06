#ifndef SCENE_CONSTANTS_HLSLI
#define SCENE_CONSTANTS_HLSLI

cbuffer FrameCB : register(b0) {
	float4x4 gView;
	float4x4 gProj;
	float4x4 gViewProj;
	float3   gCameraPos;
	float    gTime;
	float3   gFramePadding;
}

cbuffer ObjectCB : register(b1) {
	float4x4 gWorld;
	float4x4 gWorldInvTranspose;
	float4   gSkinningInfo;
}

cbuffer MaterialCB : register(b2) {
	float4 gBaseColor;
	float4 gEmissiveColor;
	float  gMetallic;
	float  gRoughness;
	float  gOpacity;
	float  gDomainMode;
	float2 gPadding;
}

cbuffer SkinningPaletteCB : register(b3) {
	float4x4 gSkinMatrices[512];
}

cbuffer ShadowCB : register(b4) {
	float4x4 gShadowLightViewProj;
	float4   gShadowParams; // x=depthBias, y=strength, z=texelSize, w=enabled
	float4   gShadowFilterParams; // x=pcfEnabled, y=pcfRadiusTexels, z=normalBias, w=unused
	float4   gDirectionToLight; // xyz=surface-to-light direction for NdotL
}

#endif
