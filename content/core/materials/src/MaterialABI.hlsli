cbuffer MaterialCB : register(b0) {
	float4 gBaseColor;
	float  gMetallic;
	float  gForceMipNorm;
	uint   gUseForcedMip;
	float  gPadding;
	float4 gExtra[14];
}

cbuffer FrameCB : register(b1) {
	float4x4 gView;
	float4x4 gProj;
	float4x4 gViewProj;
	float3   gCameraPos;
	float    gTime;
}

cbuffer ObjectCB : register(b2) {
	float4x4 gWorld;
	float4x4 gWorldInvTranspose;
}

Texture2D    gTex : register(t0);
SamplerState gLinearWrap : register(s0);

struct MatIn {
	float2 uv;
	float3 vertexColor;
};

struct MatOut {
	float3 baseColor;
	float  metallic;
};

void ShadeMaterial(in MatIn IN, out MatOut OUT);
