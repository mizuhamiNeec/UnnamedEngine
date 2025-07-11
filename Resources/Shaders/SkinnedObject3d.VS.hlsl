#include "SkinnedObject3d.hlsli"

struct TransformationMatrix {
	float4x4 WVP;
	float4x4 World;
	float4x4 WorldInverseTranspose;
};

struct BoneMatrices {
	float4x4 bones[256];
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<BoneMatrices> gBoneMatrices : register(b5);

struct VertexShaderInput {
	float4 position : POSITION0;
	float2 texcoord : TEXCOORD0;
	float3 normal : NORMAL0;
	float4 boneWeights : BONEWEIGHT0;
	uint4 boneIndices : BONEINDEX0;
};

VertexShaderOutput VSMain(VertexShaderInput input) {
	VertexShaderOutput output;
	
	// スキニング計算
	float4 skinnedPosition = float4(0, 0, 0, 0);
	float3 skinnedNormal = float3(0, 0, 0);
	
	// 最大4つのボーンによる変換を適用
	for (int i = 0; i < 4; ++i) {
		uint boneIndex = input.boneIndices[i];
		float weight = input.boneWeights[i];
		
		if (weight > 0.0f && boneIndex < 256) {
			float4x4 boneMatrix = gBoneMatrices.bones[boneIndex];
			skinnedPosition += mul(input.position, boneMatrix) * weight;
			skinnedNormal += mul(input.normal, (float3x3)boneMatrix) * weight;
		}
	}
	
	// ウェイトの合計が0の場合は元の位置を使用
	if (length(input.boneWeights) == 0.0f) {
		skinnedPosition = input.position;
		skinnedNormal = input.normal;
	}
	
	// 最終的な変換を適用
	output.position = mul(skinnedPosition, gTransformationMatrix.WVP);
	output.texcoord = input.texcoord;
	output.normal = normalize(mul(skinnedNormal, (float3x3)gTransformationMatrix.WorldInverseTranspose));
	output.worldPosition = mul(skinnedPosition, gTransformationMatrix.World).xyz;
	
	return output;
}
