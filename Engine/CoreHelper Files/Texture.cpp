#include "Texture.h"
#include "CommonFunction.h"
#include "extraPackages/d3dx12.h"

//HRESULT Texture::UpdateSubresources(
//	ID3D12Device* device,
//	ID3D12GraphicsCommandList* cmdList,
//	UINT64 intermediateOffset,
//	UINT firstSubresource)
//{
//	UINT numSubresources = static_cast<UINT>(m_texturesubresources.size());
//	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(numSubresources);
//	std::vector<UINT> numRows(numSubresources);
//	std::vector<UINT64> rowSizesInBytes(numSubresources);
//
//	UINT64 totalBytes = 0;
//	D3D12_RESOURCE_DESC desc = m_ResourceGPU->GetDesc();
//
//	device->GetCopyableFootprints(
//		&desc,
//		firstSubresource,
//		numSubresources,
//		intermediateOffset,
//		layouts.data(),
//		numRows.data(),
//		rowSizesInBytes.data(),
//		&totalBytes);
//
//	// Map upload heap
//	BYTE* mappedData = nullptr;
//	HRESULT hr = m_ResourceUpload->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
//	if (FAILED(hr))
//		return hr;
//
//	for (UINT i = 0; i < numSubresources; ++i)
//	{
//		BYTE* dstSlice = mappedData + layouts[i].Offset;
//		const BYTE* srcSlice = reinterpret_cast<const BYTE*>(m_texturesubresources[i].pData);
//
//		for (UINT y = 0; y < numRows[i]; ++y)
//		{
//			memcpy(
//				dstSlice + y * layouts[i].Footprint.RowPitch,
//				srcSlice + y * m_texturesubresources[i].RowPitch,
//				rowSizesInBytes[i]);
//		}
//	}
//
//	m_ResourceUpload->Unmap(0, nullptr);
//
//	// Issue copy commands to copy each subresource from upload heap to texture
//	for (UINT i = 0; i < numSubresources; ++i)
//	{
//		D3D12_TEXTURE_COPY_LOCATION dst = {};
//		dst.pResource = m_ResourceGPU.Get();
//		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
//		dst.SubresourceIndex = i + firstSubresource;
//
//		D3D12_TEXTURE_COPY_LOCATION src = {};
//		src.pResource = m_ResourceUpload.Get();
//		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
//		src.PlacedFootprint = layouts[i];
//
//		cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
//	}
//
//	return S_OK;
//}

