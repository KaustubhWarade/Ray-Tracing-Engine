#include "GpuBuffer.h"
#include "CommonFunction.h"
#include "../RenderEngine Files/RenderEngine.h"

void ResizableBuffer::Sync(RenderEngine* renderEngine, ID3D12GraphicsCommandList* cmdList, const void* cpuData, UINT newElementCount)
{
    GpuResourceDirty = false;

    // Case 1: The new data is empty. Release resources.
    if (newElementCount == 0)
    {
        if (Resource)
        {
            renderEngine->UnTrackResource(Resource.Get());
            Resource.Reset();
            Uploader.Reset();
            Capacity = 0;
            Size = 0;
            GpuResourceDirty = true; // The resource pointer is now null, SRVs are invalid.
        }
        return;
    }

    // Case 2: We need to grow the buffer.
    if (newElementCount > Capacity)
    {
        if (Resource)
        {
            renderEngine->UnTrackResource(Resource.Get());
        }

        UINT newCapacity = (Capacity == 0) ? 1 : Capacity;
        while (newCapacity < newElementCount)
        {
            newCapacity = (UINT)(newCapacity * 1.5f) + 1;
        }

        UINT newBufferSizeInBytes = newCapacity * Stride;
        UINT dataSizeInBytes = newElementCount * Stride;

        CreateDefaultBuffer(
            renderEngine->GetDevice(),
            cmdList,
            cpuData,
            dataSizeInBytes, 
            Uploader,
            Resource,
            newBufferSizeInBytes 
        );

        Capacity = newCapacity;
        GpuResourceDirty = true; 
    }

    // Case 3: The buffer is large enough, just update its contents (fast path).
    else
    {
        UINT dataSizeInBytes = newElementCount * Stride;
        renderEngine->TransitionResource(cmdList, Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
        update(cmdList, cpuData, dataSizeInBytes);
    }

    Size = newElementCount;
    if (Resource)
    {
        renderEngine->TransitionResource(cmdList, Resource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }
}

void ResizableBuffer::update(ID3D12GraphicsCommandList* cmdList, const void* dataToUpload, UINT sizeInBytes, UINT destOffsetInBytes)
{
    if (!Resource || !Uploader || sizeInBytes == 0) return;

    // Map, copy, unmap
    void* pMappedData = nullptr;
    Uploader->Map(0, nullptr, &pMappedData);
    memcpy(pMappedData, dataToUpload, sizeInBytes);
    Uploader->Unmap(0, nullptr);

    // Copy from uploader to the default GPU resource
    cmdList->CopyBufferRegion(
        Resource.Get(),
        destOffsetInBytes,
        Uploader.Get(),
        0,
        sizeInBytes
    );
}