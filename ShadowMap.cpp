#include "ShadowMap.h"

void ShadowMap::BuildResource() {
	D3D12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, (UINT64)width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0.0f;
	device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_GENERIC_READ, &clearValue, IID_PPV_ARGS(&shadowMap));
}

void ShadowMap::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	SRVDesc.Texture2D.MipLevels = 1;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.PlaneSlice = 0;
	SRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	device->CreateShaderResourceView(shadowMap, &SRVDesc, cpuSRV);

	D3D12_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
	DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DSVDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DSVDesc.Texture2D.MipSlice = 0;
	DSVDesc.Flags = D3D12_DSV_FLAG_NONE;
	device->CreateDepthStencilView(shadowMap, &DSVDesc, cpuDSV);
}

void ShadowMap::SetLightTransformMatrix(DirectX::XMFLOAT3 lightDirection) {
	DirectX::XMVECTOR lightVec = DirectX::XMLoadFloat3(&lightDirection);
	DirectX::XMVECTOR lightPos = DirectX::XMVectorScale(lightVec, -40.0f);
	DirectX::XMVECTOR targetPos = DirectX::XMVectorZero();
	DirectX::XMVECTOR lightUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	DirectX::XMMATRIX lightView = DirectX::XMMatrixLookAtLH(lightPos, targetPos, lightUp);
	DirectX::XMStoreFloat4x4(&this->lightView, lightView);

	DirectX::XMMATRIX lightProj = DirectX::XMMatrixOrthographicLH(75.0f, 75.0f, 0.0f, 100.0f);
	DirectX::XMStoreFloat4x4(&this->lightProj, lightProj);

	/*Shadow Transform Matrix*/
	DirectX::XMMATRIX texMatrix(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	);

	DirectX::XMMATRIX result = DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(lightView, lightProj), texMatrix);
	DirectX::XMStoreFloat4x4(&shadowTransform, result);
}