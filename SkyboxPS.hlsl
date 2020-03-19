struct Input
{
	float4 position : SV_POSITION;
	float3 posL : POSL;
};

TextureCube tex : register(t0);
SamplerState samplerState : register(s0);

float4 main(Input input) : SV_TARGET
{
	return tex.Sample(samplerState, input.posL);
}