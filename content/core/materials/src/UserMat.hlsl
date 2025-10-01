// こいつはサジェスト用。 編集が完了したらコメントアウトか、削除しよう
//#include "MaterialABI.hlsli"

void ShadeMaterial(in MatIn IN, out MatOut OUT) {
	float2 uv = float2(IN.uv.x, -IN.uv.y);

	uint tw, th, mipCount;
	gTex.GetDimensions(0, tw, th, mipCount);

	float4 texColor;
	if (gUseForcedMip != 0) {
		uint targetMip = (mipCount - 1) ?
			                 (uint)round(
				                 saturate(gForceMipNorm) * (mipCount - 1)) :
			                 0;
		texColor = gTex.SampleLevel(gLinearWrap, uv, targetMip);
	} else {
		texColor = gTex.Sample(gLinearWrap, uv);
	}

	float3 tex    = texColor.rgb;
	float  rough  = gExtra[0].x;
	float3 emi    = gExtra[1].rgb;
	float3 lit = lerp(tex * 0.6, tex, saturate(1 - gMetallic));
	OUT.baseColor =  lit * gBaseColor.rgb * IN.vertexColor.rgb;
	OUT.metallic  = gMetallic;
}
