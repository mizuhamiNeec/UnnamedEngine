#include "../include/fixture_quoted.hlsli"
#include <shaders/include/fixture_angle.hlsli>

struct VsInput {
	float3 position : POSITION;
};

struct VsOutput {
	float4 position : SV_POSITION;
};

VsOutput VsMain(VsInput input) {
	VsOutput output;
	output.position = float4(input.position, 1.0f);
	return output;
}

float4 PsMain(VsOutput input) : SV_TARGET {
	return float4(kFixtureColor, kFixtureAlpha);
}
