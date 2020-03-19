#pragma once
#include "d3dUtil.h"
#include "UploadBuffer.h"

class FrameResource {
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount, UINT skinnedObjectCount);
	~FrameResource(){}

	ID3D12CommandAllocator* cmdAlloc = nullptr;

	std::unique_ptr<UploadBuffer<PassConstants>> passCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> objCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstants>> matCB = nullptr;
	std::unique_ptr<UploadBuffer<SkinnedConstants>> skinnedCB = nullptr;

	UINT64 fence = 0;
};