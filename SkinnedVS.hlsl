struct Light {
	float3 strength;
	float fallOffStart;					   //point/spot light only
	float3 direction;					   //directional/spot light only
	float fallOffEnd;					   //point/spot light only
	float3 position;					   //point/spot light only
	float spotPower;					   //spot light only
};

struct Output
{
	float4 position : SV_POSITION;
	float3 posW : POSW;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD;
	float4 shadowPos : SHADOWPOS;
};

struct ObjectConstants {
	float4x4 worldMatrix;
};

struct SkinnedConstants {
	float4x4 boneTransforms[600];
};

struct PassConstants {
	float4x4 viewMatrix;
	float4x4 projMatrix;
	float4x4 shadowTransform;

	float4 eyePos;
	float4 ambientLight;

	Light lights[1];
};

struct Input {
	float3 position : POSITION;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;

	float3 boneWeights : WEIGHTS;
	uint4 boneIndices : BONEINDICES;
};

ConstantBuffer<ObjectConstants> objCB : register(b0);
ConstantBuffer<PassConstants> passCB : register(b1);
ConstantBuffer<SkinnedConstants> skinnedCB : register(b3);

Output main(Input input)
{
	Output output;

	float3 posL = float3(0.0f, 0.0f, 0.0f);    //Local Space
	float3 normalL = float3(0.0f, 0.0f, 0.0f);

	/*Bone Animation*/
	float weights[4] = { input.boneWeights.x, input.boneWeights.y, input.boneWeights.z, 1.0f - input.boneWeights.x - input.boneWeights.y - input.boneWeights.z };

	for (int i = 0; i < 4; i++) {
		posL += weights[i] * mul(float4(input.position, 1.0f), skinnedCB.boneTransforms[input.boneIndices[i]]).xyz;
		normalL += weights[i] * mul(float4(input.normal, 1.0f), (float3x3)skinnedCB.boneTransforms[input.boneIndices[i]]);
	}

	float4 transformPosition = mul(float4(posL, 1.0f), objCB.worldMatrix);
	float4x4 MVP = mul(passCB.viewMatrix, passCB.projMatrix);

	output.posW = transformPosition.xyz;
	output.position = mul(transformPosition, MVP);
	output.normal = normalL;
	output.texCoord = input.texCoord;
	output.shadowPos = mul(transformPosition, passCB.shadowTransform);

	return output;
}