#include "DescriptorTable.h"

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorTable::GetCpuHandle(UINT slot) const
{
    assert(IsValid() && "Attempting to access an invalid descriptor table.");
    assert(slot < m_numDescriptors && "Descriptor table slot index out of bounds.");

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_baseCpuHandle;
    handle.ptr += (size_t)slot * m_descriptorSize;
    return handle;
}
