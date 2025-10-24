#pragma once
#include "../RenderEngine Files/global.h"

class DescriptorTable
{
public:
    DescriptorTable() = default;

    // Checks if the table points to a valid allocation.
    bool IsValid() const { return m_baseCpuHandle.ptr != 0; }

    // Returns the CPU handle for a specific slot in the table.
    D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(UINT slot) const;

    // Returns the GPU handle for the start of the table (for Set...RootDescriptorTable).
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle() const { return m_baseGpuHandle; }

private:
    // Only the allocator that created this table can set its members.
    friend class DescriptorAllocator;

    D3D12_CPU_DESCRIPTOR_HANDLE m_baseCpuHandle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE m_baseGpuHandle = {};
    UINT m_descriptorSize = 0;
    UINT m_numDescriptors = 0;
};