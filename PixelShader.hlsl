/*======================================LightingUtil===============================================*/

struct Light {
	float3 strength;
	float fallOffStart;					   //point/spot light only
	float3 direction;					   //directional/spot light only
	float fallOffEnd;					   //point/spot light only
	float3 position;					   //point/spot light only
	float spotPower;					   //spot light only
};

struct Material {
	float4 diffuseAlbedo;
	float3 fresnelR0;
	float shininess;
};

/*����˥������*/
float CalcAttenuation(float distance, float fallOffStart, float fallOffEnd) {
	//����˥��
	return saturate((fallOffEnd - distance) / (fallOffEnd - fallOffStart));
}

/*Schlick���Ʒ�����Fresnel����*/
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec) {
	//���㷨�����͹������ļн�����ֵ
	float cosIncidentAngle = saturate(dot(normal, lightVec));
	//Schlick���Ʒ���
	float f0 = 1.0f - cosIncidentAngle;
	float3 reflectPercent = R0 + (1.0f - R0) * pow(f0, 5);
	return reflectPercent;
}

/*���Ϲ���ģ��*/
float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat) {
	const float shininess = mat.shininess * 256.0f;
	//Ϊ�������ֲڶ� ����������
	float3 halfVec = normalize(toEye + lightVec);
	//���������ֲڶ�
	float roughnessFactor = (shininess + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), shininess) / 8.0f;
	//Fresnel����(���������Ϊ������)
	float3 fresnelFactor = SchlickFresnel(mat.fresnelR0, halfVec, lightVec);
	//��������淴��ֵ
	float3 specAlbedo = fresnelFactor * roughnessFactor;
	//������С���淴���ֵ(LDR)
	specAlbedo = specAlbedo / (specAlbedo + 1.0f);
	//������ + ���淴�� * ��ǿ
	return (mat.diffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

/*�������㺯��*/
float3 ComputeDirectionalLight(Light light, Material mat, float3 normal, float3 toEye) {
	float3 lightVec = -light.direction;
	//��Lambert���Ҷ�����С��ǿ
	float lambertFactor = max(dot(lightVec, normal), 0.0f);
	float3 lightStrength = light.strength * lambertFactor;

	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

/*���Դ���㺯��*/
float3 ComputePointLight(Light light, Material mat, float3 pos, float3 normal, float3 toEye) {
	float3 lightVec = light.position - pos;

	//����͹�Դ�ľ���
	float distance = length(lightVec);
	if (distance > light.fallOffEnd)
		return 0.0f; //����˥����Χ�������չ���
	lightVec /= distance;

	//��Lambert���Ҷ�����С��ǿ
	float lambertFactor = max(dot(lightVec, normal), 0.0f);
	float3 lightStrength = light.strength * lambertFactor;

	//��������˥��
	float att = CalcAttenuation(distance, light.fallOffStart, light.fallOffEnd);
	lightStrength *= att;

	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

/*�۹�Ƽ��㺯��*/
float3 ComputeSpotLight(Light light, Material mat, float3 pos, float3 normal, float3 toEye) {
	float3 lightVec = light.position - pos;

	//����͹�Դ�ľ���
	float distance = length(lightVec);
	if (distance > light.fallOffEnd)
		return 0.0f; //����˥����Χ�������չ���
	lightVec /= distance;

	//��Lambert���Ҷ�����С��ǿ
	float lambertFactor = max(dot(lightVec, normal), 0.0f);
	float3 lightStrength = light.strength * lambertFactor;

	//��������˥��
	float att = CalcAttenuation(distance, light.fallOffStart, light.fallOffEnd);
	lightStrength *= att;

	//����۹��˥��
	float spotFactor = pow(max(dot(-lightVec, light.direction), 0.0f), light.spotPower);
	lightStrength *= spotFactor;

	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

/*===================================Shadow Mapping===========================================*/

Texture2D shadowMap : register(t1);
SamplerComparisonState shadowSampler : register(s1);

float CalcShadowFactor(float4 shadowPos) {
	shadowPos.xyz /= shadowPos.w;

	float depth = shadowPos.z;
	uint width, height, numMips;
	shadowMap.GetDimensions(0, width, height, numMips);
	float dx = 1.0f / (float)width;
	float percentLit = 0.0f;

	const float2 offsets[9] = {
		float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
		float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
		float2(-dx, dx), float2(0, dx), float2(dx, dx)
	};

	for (int i = 0; i < 9; i++)
		percentLit += shadowMap.SampleCmpLevelZero(shadowSampler, shadowPos.xy + offsets[i], depth).r;
	return percentLit / 9.0f;
}

/*============================================================================================*/

struct Input
{
	float4 position : SV_POSITION;
	float3 posW : POSW;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD;
	float4 shadowPos : SHADOWPOS;
};

struct PassConstants {
	float4x4 viewMatrix;
	float4x4 projMatrix;
	float4x4 shadowTransform;

	float4 eyePos;
	float4 ambientLight;
	
	Light lights[1];
};

struct MaterialConstants {
	float4 diffuseAlbedo;
	float3 fresnelR0;
	float roughness;
	float4x4 matTransform;
};

Texture2D tex : register(t0);
SamplerState samplerState : register(s0);

ConstantBuffer<PassConstants> passCB : register(b1);
ConstantBuffer<MaterialConstants> matCB : register(b2);

float4 main(Input input) : SV_TARGET
{
	float4 sampleColor = tex.Sample(samplerState, input.texCoord);

	float3 eyePos = passCB.eyePos.xyz;

	input.normal = normalize(input.normal);
	float3 toEye = normalize(eyePos - input.posW);

	float4 diffuse = sampleColor * matCB.diffuseAlbedo;

	const float shininess = 1.0f - matCB.roughness;
	Material mat = { matCB.diffuseAlbedo, matCB.fresnelR0, shininess };

	float shadowFactor = CalcShadowFactor(input.shadowPos);

	float3 lightingResult = shadowFactor * ComputeDirectionalLight(passCB.lights[0], mat, input.normal, toEye);

	float4 litColor = diffuse * (passCB.ambientLight + float4(lightingResult, 1.0f));
	litColor.a = diffuse.a;

	return litColor;
}