#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount, UINT skinnedObjectCount) {
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
	passCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
	objCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
	matCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, materialCount, true);
	skinnedCB = std::make_unique<UploadBuffer<SkinnedConstants>>(device, skinnedObjectCount, true);
}