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
	float3 posL : POSL;
};

struct ObjectConstants {
	float4x4 worldMatrix;
};

struct PassConstants {
	float4x4 viewMatrix;
	float4x4 projMatrix;
	float4x4 shadowTransform;

	float4 eyePos;
	float4 ambientLight;

	Light lights[1];
};

ConstantBuffer<ObjectConstants> objCB : register(b0);
ConstantBuffer<PassConstants> passCB : register(b1);

Output main(float3 position : POSITION, float3 normal : NORMAL, float2 texCoord : TEXCOORD)
{
	Output output;

	//�þֲ������λ����Ϊ������ͼ�Ĳ�������
	output.posL = position;

	//�Ѷ���任����βü��ռ�
	float4 posW = mul(float4(position, 1.0f), objCB.worldMatrix);

	//�������������Ϊ����������
	posW.xyz += passCB.eyePos;

	float4x4 MVP = mul(passCB.viewMatrix, passCB.projMatrix);
	posW = mul(posW, MVP);

	//ʹz = w ����������λ��Զƽ��
	output.position = posW.xyww;

	return output;
}