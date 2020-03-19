#pragma once

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tchar.h>
#include <dxgi1_6.h>
#include <wrl.h>
using namespace Microsoft;

#include <DirectXMath.h>
#include <DirectXCollision.h>

#include <d3d12.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

#include "d3dx12.h"

#include <dinput.h>
#pragma comment(lib, "dinput8.lib")

#include <unordered_map>

#define CalcUpper(byteSize) ((byteSize + 255) & ~255)

#define NUM_BONES_PER_VERTEX 4
#define MAX_BONE_NUM 600

/*数据存储结构体*/
struct Light {
	DirectX::XMFLOAT3 strength;
	float fallOffStart;
	DirectX::XMFLOAT3 direction;
	float fallOffEnd;
	DirectX::XMFLOAT3 position;
	float spotPower;
};

struct ObjectConstants {
	DirectX::XMFLOAT4X4 worldMatrix;
	DirectX::XMFLOAT4X4 texTransform;
};

struct SkinnedConstants {
	DirectX::XMFLOAT4X4 boneTransforms[MAX_BONE_NUM];
};

struct PassConstants {
	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projMatrix;
	DirectX::XMFLOAT4X4 shadowTransform;

	DirectX::XMFLOAT4 eyePos;
	DirectX::XMFLOAT4 ambientLight;

	Light lights[1];
};

struct MaterialConstants {
	DirectX::XMFLOAT4 diffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 fresnelR0 = { 0.01f, 0.01f, 0.01f };
	float roughness = 0.25f;

	DirectX::XMFLOAT4X4 matTransform = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
};

struct Vertex {
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT2 texCoord;
	DirectX::XMFLOAT3 normal;
};

struct SkinnedVertex {
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT2 texCoord;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT3 boneWeights;
	BYTE boneIndices[NUM_BONES_PER_VERTEX];
};

struct Material {
	std::string name;

	int matCBIndex = -1;
	int diffuseSRVIndex = -1;
	int normalSRVIndex = -1;

	DirectX::XMFLOAT4 diffuseAlbedo = { 1.0f,1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 frensnelR0 = { 0.01f, 0.01f, 0.01f };
	float roughness = 0.25f;

	DirectX::XMFLOAT4X4 matTransform = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};

	UINT numFramesDirty;
};

/*几何体辅助结构体*/
struct SubmeshGeometry {
	UINT indexCount = 0;
	UINT startIndexLocation = 0;
	int baseVertexLocation = 0;
};

struct MeshGeometry {
	std::string name;

	ID3DBlob* vertexBufferCPU;
	ID3DBlob* indexBufferCPU;
	ID3D12Resource* vertexBufferGPU;
	ID3D12Resource* indexBufferGPU;

	UINT vertexBufferStride = 0;
	UINT vertexBufferSize = 0;
	DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT;
	UINT indexBufferSize = 0;

	std::vector<SubmeshGeometry> drawArgs;

	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() {
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = vertexBufferGPU->GetGPUVirtualAddress();
		vbv.SizeInBytes = vertexBufferSize;
		vbv.StrideInBytes = vertexBufferStride;
		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() {
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = indexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = indexFormat;
		ibv.SizeInBytes = indexBufferSize;
		return ibv;
	}
};