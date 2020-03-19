#pragma once
#include "d3dUtil.h"

class ShadowMap {
public:
	ShadowMap(ID3D12Device* device, UINT width, UINT height) {
		this->device = device;
		this->width = width;
		this->height = height;
		viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
		scissorRect = { 0, 0, (int)width, (int)height };

		BuildResource();
	}

	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE cpuSRV, CD3DX12_GPU_DESCRIPTOR_HANDLE gpuSRV, CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDSV) {
		this->cpuDSV = cpuDSV;
		this->cpuSRV = cpuSRV;
		this->gpuSRV = gpuSRV;

		BuildDescriptors();
	}

	void SetLightTransformMatrix(DirectX::XMFLOAT3 lightDirection);

	ID3D12Resource* GetResource() { return shadowMap; }
	D3D12_VIEWPORT GetViewport() { return viewport; }
	D3D12_RECT GetScissorRect() { return scissorRect; }
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetSRV() { return gpuSRV; }
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDSV() { return cpuDSV; }
	DirectX::XMFLOAT4X4 GetViewMatrix() { return lightView; }
	DirectX::XMFLOAT4X4 GetProjMatrix() { return lightProj; }
	DirectX::XMFLOAT4X4 GetShadowTranformMatrix() { return shadowTransform; }

private:
	ID3D12Device* device;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;

	UINT width, height;
	DXGI_FORMAT format = DXGI_FORMAT_R24G8_TYPELESS;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDSV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuSRV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuSRV;

	ID3D12Resource* shadowMap = nullptr;

	DirectX::XMFLOAT4X4 lightView;
	DirectX::XMFLOAT4X4 lightProj;
	DirectX::XMFLOAT4X4 shadowTransform;
		
	void BuildResource();
	void BuildDescriptors();
};