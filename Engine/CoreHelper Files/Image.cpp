#include "Image.h"
#include "CommonFunction.h"
#include "extraPackages/d3dx12.h"

void Image::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
	m_device = device;
	m_cmdList= cmdList;
}

//Image::~Image() noexcept
//{
//	if (m_data)
//	{
//		delete[] m_data;
//		m_data = nullptr;
//	}
//	if (m_accumulateData)
//	{
//		delete[] m_accumulateData;
//		m_accumulateData = nullptr;
//	}
//}

HRESULT Image::CreateImageResource(
	UINT width, UINT height, 
	D3D12_RESOURCE_STATES startState = D3D12_RESOURCE_STATE_COMMON,
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM,
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
{
	m_width = width;
	m_height = height;
	m_startState = startState;
	m_format = format;
	m_flags = flags;

	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = m_width;
	texDesc.Height = m_height;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = m_format;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = m_flags;

	ComPtr<ID3D12Resource> textureGPU;
	D3D12_HEAP_PROPERTIES heapProperties;
	CreateHeapProperties(heapProperties, D3D12_HEAP_TYPE_DEFAULT);
	EXECUTE_AND_LOG_RETURN(m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		m_startState,
		nullptr,
		IID_PPV_ARGS(&m_imageGPU)));
	m_imageGPU->SetName(L"FinalTextureGPU Resource");
	m_currentResourceState = m_startState;

	UINT64 uploadSize = 0;
	// D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
	m_device->GetCopyableFootprints(&texDesc, 0, 1, 0, &m_footprint, nullptr, nullptr, &uploadSize);

	D3D12_RESOURCE_DESC uploadDesc = {};
	uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploadDesc.Alignment = 0;
	uploadDesc.Width = uploadSize;
	uploadDesc.Height = 1;
	uploadDesc.DepthOrArraySize = 1;
	uploadDesc.MipLevels = 1;
	uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
	uploadDesc.SampleDesc.Count = 1;
	uploadDesc.SampleDesc.Quality = 0;
	uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	uploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	CreateHeapProperties(heapProperties, D3D12_HEAP_TYPE_UPLOAD);
	ComPtr<ID3D12Resource> textureUpload;
	EXECUTE_AND_LOG_RETURN(m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&uploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_imageUpload)));
	m_imageUpload->SetName(L"FinalTextureUpload Resource");

	m_data = new uint32_t[width * height];
	m_accumulateData = new XMFLOAT4[width * height];

	return S_OK;
}

void Image::Resize(UINT newWidth, UINT newHeight)
{
	if (newWidth == m_width && newHeight == m_height)
		return; // No need to recreate

	// Release old resources
	m_imageGPU.Reset();
	m_imageUpload.Reset();

	if (m_data)
	{
		delete[] m_data;
		m_data = nullptr;
	}
	if (m_accumulateData)
	{
		delete[] m_accumulateData;
		m_accumulateData = nullptr;
	}

	// Recreate the texture with new size
	CreateImageResource(newWidth, newHeight, m_startState, m_format, m_flags);
}

void Image::SetData(const uint32_t *data)
{
	uint8_t *mapped = nullptr;
	D3D12_RANGE readRange = {0, 0};
	m_imageUpload->Map(0, &readRange, reinterpret_cast<void **>(&mapped));
	// uint32_t* dst = reinterpret_cast<uint32_t*>(mapped);
	UINT rowPitchPixels = m_footprint.Footprint.RowPitch / sizeof(uint32_t);
	TransitionResource(m_cmdList, m_imageGPU.Get(),
		m_currentResourceState,
							 D3D12_RESOURCE_STATE_COPY_DEST);
	m_currentResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
	for (UINT y = 0; y < m_height; ++y)
	{
		memcpy(
			mapped + y * m_footprint.Footprint.RowPitch,
			data + y * m_width,
			m_width * sizeof(uint32_t));
	}
	m_imageUpload->Unmap(0, nullptr);

	D3D12_TEXTURE_COPY_LOCATION dst = {};
	dst.pResource = m_imageGPU.Get();
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.pResource = m_imageUpload.Get();
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src.PlacedFootprint = m_footprint;

	m_cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

	TransitionResource(m_cmdList, m_imageGPU.Get(),
		m_currentResourceState,
							 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_currentResourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}


HRESULT Texture::UpdateSubresources(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	UINT64 intermediateOffset,
	UINT firstSubresource)
{
	UINT numSubresources = static_cast<UINT>(m_texturesubresources.size());
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(numSubresources);
	std::vector<UINT> numRows(numSubresources);
	std::vector<UINT64> rowSizesInBytes(numSubresources);

	UINT64 totalBytes = 0;
	D3D12_RESOURCE_DESC desc = m_ResourceGPU->GetDesc();

	device->GetCopyableFootprints(
		&desc,
		firstSubresource,
		numSubresources,
		intermediateOffset,
		layouts.data(),
		numRows.data(),
		rowSizesInBytes.data(),
		&totalBytes);

	// Map upload heap
	BYTE* mappedData = nullptr;
	HRESULT hr = m_ResourceUpload->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
	if (FAILED(hr))
		return hr;

	for (UINT i = 0; i < numSubresources; ++i)
	{
		BYTE* dstSlice = mappedData + layouts[i].Offset;
		const BYTE* srcSlice = reinterpret_cast<const BYTE*>(m_texturesubresources[i].pData);

		for (UINT y = 0; y < numRows[i]; ++y)
		{
			memcpy(
				dstSlice + y * layouts[i].Footprint.RowPitch,
				srcSlice + y * m_texturesubresources[i].RowPitch,
				rowSizesInBytes[i]);
		}
	}

	m_ResourceUpload->Unmap(0, nullptr);

	// Issue copy commands to copy each subresource from upload heap to texture
	for (UINT i = 0; i < numSubresources; ++i)
	{
		D3D12_TEXTURE_COPY_LOCATION dst = {};
		dst.pResource = m_ResourceGPU.Get();
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = i + firstSubresource;

		D3D12_TEXTURE_COPY_LOCATION src = {};
		src.pResource = m_ResourceUpload.Get();
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.PlacedFootprint = layouts[i];

		cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	}

	return S_OK;
}

HRESULT Texture::loadTexture(ID3D12Device* pDevice,ID3D12GraphicsCommandList* pCmdList, const wchar_t* filename)
{
	HRESULT hr = S_OK;

	hr = DirectX::LoadDDSTextureFromFile(
		pDevice,
		filename,
		m_ResourceGPU.GetAddressOf(),
		m_textureddsData,
		m_texturesubresources);

	if (FAILED(hr) || !m_ResourceGPU || m_texturesubresources.empty())
	{
		fprintf(gpFile, "LoadDDSTextureFromFile() Failed. HRESULT: 0x%08X\n", hr);
		return hr;
	}
	else
	{
		D3D12_RESOURCE_DESC desc = m_ResourceGPU->GetDesc();
		fprintf(gpFile, "Format: %d, Width: %llu, Height: %u, MipLevels: %d, Dimension: %d\n",
			desc.Format, desc.Width, desc.Height, desc.MipLevels, desc.Dimension);
	}

	UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_ResourceGPU.Get(), 0, static_cast<UINT>(m_texturesubresources.size()));

	D3D12_HEAP_PROPERTIES heapProps = {};
	CreateHeapProperties(heapProps, D3D12_HEAP_TYPE_UPLOAD);

	D3D12_RESOURCE_DESC resourceDesc = {};
	ZeroMemory(&resourceDesc, sizeof(D3D12_RESOURCE_DESC));
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = uploadBufferSize; // The size of the buffer in bytes
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	EXECUTE_AND_LOG_RETURN(pDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, // Initial state for an upload heap
		nullptr,
		IID_PPV_ARGS(&m_ResourceUpload)));

	UpdateSubresources(
		pDevice,
		pCmdList,
		0, 0);

	TransitionResource(
		pCmdList,
		m_ResourceGPU.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

}

