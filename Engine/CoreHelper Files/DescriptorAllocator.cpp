#include "DescriptorAllocator.h"
#include "DescriptorTable.h"

HRESULT DescriptorAllocator::Initialize(ID3D12Device* device, ID3D12DescriptorHeap* pHeap, UINT baseIndex, UINT capacity)
{
    m_heap = pHeap;
    m_capacity = capacity;

    m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();
    m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();
    m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    m_cpuStart.ptr = m_cpuStart.ptr + (SIZE_T)baseIndex * m_descriptorSize;
    m_gpuStart.ptr = m_gpuStart.ptr + (SIZE_T)baseIndex * m_descriptorSize;

    m_freeList.reserve(capacity);
    for (UINT i = 0; i < capacity; ++i)
    {
        m_freeList.push_back(i);
    }

    return S_OK;
}

DescriptorHandle DescriptorAllocator::AllocateSingle()
{
    assert(!m_freeList.empty() && "Descriptor heap is full! Cannot allocate single descriptor.");

    UINT indexInAllocator = m_freeList.back();
    m_freeList.pop_back();

    DescriptorHandle handle;
    handle.CpuHandle.ptr = m_cpuStart.ptr + (SIZE_T)indexInAllocator * m_descriptorSize;
    handle.GpuHandle.ptr = m_gpuStart.ptr + (SIZE_T)indexInAllocator * m_descriptorSize;
    handle.HeapIndex = indexInAllocator;

    return handle;
}

void DescriptorAllocator::FreeSingle(DescriptorHandle& handle)
{
    if (!handle.IsValid()) return;

    m_freeList.push_back(handle.HeapIndex);

    handle = {};
}


DescriptorTable DescriptorAllocator::AllocateTable(UINT count)
{
    assert(m_freeList.size() >= count && "Descriptor heap is full! Not enough free descriptors to allocate.");

    UINT baseOffsetInAllocator = m_freeList.back() - (count - 1);
    for (UINT i = 0; i < count; ++i) {
        m_freeList.pop_back();
    }

    DescriptorTable table;
    table.m_baseCpuHandle.ptr = m_cpuStart.ptr + (SIZE_T)baseOffsetInAllocator * m_descriptorSize;
    table.m_baseGpuHandle.ptr = m_gpuStart.ptr + (SIZE_T)baseOffsetInAllocator * m_descriptorSize;
    table.m_descriptorSize = m_descriptorSize;
    table.m_numDescriptors = count;

    return table;
}

void DescriptorAllocator::FreeTable(DescriptorTable& table)
{
    if (!table.IsValid()) return;

    UINT offsetFromAllocatorStart = (UINT)((table.m_baseCpuHandle.ptr - m_cpuStart.ptr) / m_descriptorSize);

    for (UINT i = 0; i < table.m_numDescriptors; ++i)
    {
        m_freeList.push_back(offsetFromAllocatorStart + table.m_numDescriptors - 1 - i);
    }

    table = {};
}