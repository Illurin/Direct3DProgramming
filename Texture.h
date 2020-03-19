#pragma once
#include <wincodec.h>
#include "d3dUtil.h"

struct ImageInfo {
	UINT width, height, BPP;
	IWICBitmapSource* source;
};

class Texture {
public:
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

	void LoadImageInfo(ImageInfo imageInfo);
	void InitBuffer(ID3D12Device* device);
	void InitBuffer(ID3D12Device* device, UINT offsetX, UINT offsetY, UINT width, UINT height);
	void InitBufferAsCubeMap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
	void SetCopyCommand(ID3D12GraphicsCommandList* cmdList);
	ID3D12Resource* GetTexture();

private:
	UINT width, height, BPP;
	ID3D12Resource* resource;
	IWICBitmapSource* source;
	ID3D12Resource* uploader;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
};

bool LoadPixelWithWIC(const wchar_t* path, GUID tgFormat, ImageInfo& imageInfo);