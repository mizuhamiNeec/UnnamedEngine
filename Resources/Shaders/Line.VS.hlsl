#include "Line.hlsli"

struct TransformationMatrix {
	float4x4 WVP;
	float4x4 World;
};

struct VertexShaderInput {
	float3 position : POSITION;
	float4 color : COLOR;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

VertexShaderOutput main(VertexShaderInput input) {
	VertexShaderOutput output;
	output.position = mul(input.position, gTransformationMatrix.WVP);
	output.color = input.color;
	return output;
}
