#include "Object3d.hlsli"

struct Material
{
	float32_t4 color;
	int32_t enableLighting;
	float32_t4x4 uvTransform;
};

struct DirectionalLight
{
	float32_t4 color; //!< ライトの色
	float32_t3 direction; //!< ライトの向き
	float intensity; //!< 輝度
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
	float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
	PixelShaderOutput output;
	float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
	float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
	if (gMaterial.enableLighting != 0) // ライティングを有効にする場合
	{
		float cos = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
		output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
	} else
	{
		// ライティングを無効にする場合
		output.color = gMaterial.color * textureColor;
	}

	//output.color = pow(output.color, 1 / 2.2); // sRGB

	return output;
}
