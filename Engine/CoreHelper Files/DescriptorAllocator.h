#pragma once
#include "../RenderEngine Files/global.h"

struct DescriptorAllocation
{
    D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle{};
};


class DescriptorAllocator
{
public:
    DescriptorAllocator() = default;

    // Initializes the allocator with a heap of a specific size.
    HRESULT Create(ID3D12Device* device);

    // Allocates a block of descriptors.
    DescriptorAllocation Allocate(UINT count = 1);

    // Returns the underlying descriptor heap.
    ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }

private:
    ComPtr<ID3D12DescriptorHeap> m_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart{};
    UINT m_descriptorSize = 0;
    UINT m_capacity = 0;
    UINT m_currentSize = 0; // The number of descriptors currently allocated.
};