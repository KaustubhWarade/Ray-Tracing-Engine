#pragma once
#include "../RenderEngine Files/global.h"

class DescriptorTable;

struct DescriptorHandle
{
    D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle{};
    int HeapIndex = -1;

    bool IsValid() const { return CpuHandle.ptr != 0; }
};

class DescriptorAllocator
{
public:
    DescriptorAllocator() = default;

    // Initializes the allocator with a heap of a specific size.
    HRESULT Initialize(ID3D12Device* device, ID3D12DescriptorHeap* pHeap, UINT baseIndex, UINT capacity);

    // Allocates a descriptor table.
    DescriptorTable AllocateTable(UINT count);
    void FreeTable(DescriptorTable& table);

    DescriptorHandle AllocateSingle();
    void FreeSingle(DescriptorHandle& handle);

    ID3D12DescriptorHeap* GetHeap() { return m_heap; };
    UINT GetDescriptorSize() const { return m_descriptorSize; }


private:
    ID3D12DescriptorHeap* m_heap = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart{};
    UINT m_descriptorSize = 0;
    UINT m_capacity = 0;

    std::vector<UINT> m_freeList;
};