#include "Skybox.hlsli"

struct Material {
    float4 baseColor;
    float  metallic;
    float  roughness;
    float3 emissive;
};

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

ConstantBuffer<Material> gMaterial : register(b1);
TextureCube<float4>      gTexture : register(t0);
SamplerState             gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    float4            textureColor = gTexture.Sample(gSampler, input.texcoord);
    output.color                   = textureColor * gMaterial.baseColor;
    return output;
}
