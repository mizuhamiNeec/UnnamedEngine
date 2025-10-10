#include "Skybox.hlsli"

struct TransformationMatrix {
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    // VertexShaderの出力は同時クリップ空間であり、
    // 同時クリップ空間で最も遠い場所はwである。
    // その後のPerspectiveDivideによってNDCになることを考えても、
    // zとwを同じ値にしておけば、z=1となり、NDCでの最遠方に配置されることになる。
    output.position = mul(input.position, gTransformationMatrix.WVP).xyww;
    output.texcoord = input.position.xyz;
    return output;
}
