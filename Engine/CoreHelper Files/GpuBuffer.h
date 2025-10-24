#pragma once

#include "../RenderEngine Files/global.h"

class RenderEngine;

struct ResizableBuffer
{
    ComPtr<ID3D12Resource> Resource;
    ComPtr<ID3D12Resource> Uploader;

    UINT Size = 0;      
    UINT Capacity = 0;  
    UINT Stride = 0;    
    bool GpuResourceDirty = false; 

    void Sync(RenderEngine* renderEngine, ID3D12GraphicsCommandList* cmdList, const void* cpuData, UINT newElementCount);

    void update(ID3D12GraphicsCommandList* cmdList, const void* dataToUpload, UINT sizeInBytes, UINT destOffsetInBytes = 0);
};