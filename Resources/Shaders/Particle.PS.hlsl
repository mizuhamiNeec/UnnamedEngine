#include "Particle.hlsli"

struct Material {
	float4 color;
	int enableLighting;
	float4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
	float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
	PixelShaderOutput output;
	float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
	float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
	output.color = gMaterial.color * textureColor * input.color;
	if (output.color.a == 0.0f) {
		discard;
	}

	//output.color = pow(output.color, 1 / 2.2); // sRGB

	return output;
}
