struct SkyboxVsIn {
	float3 pos : POSITION;
	float3 nrm : NORMAL;
	float2 uv : TEXCOORD0;
	float4 tangent : TANGENT;
	float4 boneIndices : TEXCOORD1;
	float4 boneWeights : TEXCOORD2;
};

struct SkyboxVsOut {
	float4 position : SV_POSITION;
	float3 direction : TEXCOORD0;
};
