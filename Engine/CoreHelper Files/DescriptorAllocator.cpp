#include "DescriptorAllocator.h"

HRESULT DescriptorAllocator::Create(ID3D12Device* device)
{
    m_capacity = 1000;
    m_currentSize = 0;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1000;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    
    EXECUTE_AND_LOG_RETURN(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_heap)));

    m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();
    m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();
    m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

DescriptorAllocation DescriptorAllocator::Allocate(UINT count)
{
    //assert(m_currentSize + count <= m_capacity && "Descriptor heap is full!");

    UINT startIndex = m_currentSize;
    m_currentSize += count;

    DescriptorAllocation allocation;
    allocation.CpuHandle.ptr = m_cpuStart.ptr + (SIZE_T)startIndex * m_descriptorSize;
    allocation.GpuHandle.ptr = m_gpuStart.ptr + (SIZE_T)startIndex * m_descriptorSize;

    return allocation;
}