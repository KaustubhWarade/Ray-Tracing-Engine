#include "Image.h"
#include "CommonFunction.h"
#include "extraPackages/d3dx12.h"
#include "ResourceManager.h"


Image::Image() {}

Image::~Image()
{
	delete[] m_pixelData;
	m_pixelData = nullptr;

	delete[] m_accumulateData;
	m_accumulateData = nullptr;

	m_gpuResource.Reset();
	m_uploadBuffer.Reset();
}

HRESULT Image::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, DescriptorAllocator* allocator,
	UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags)
{
	m_device = device;
	m_cmdList = cmdList;
	m_width = width;
	m_height = height;
	m_flags = flags;
	m_format = format;

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
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&m_gpuResource)));
	m_gpuResource->SetName(L"FinalTextureGPU Resource");

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
		IID_PPV_ARGS(&m_uploadBuffer)));
	m_uploadBuffer->SetName(L"FinalTextureUpload Resource");

	if (m_pixelData) delete[] m_pixelData;
	m_pixelData = new uint32_t[width * height];
	ZeroMemory(m_pixelData, width * height * sizeof(uint32_t));
	m_accumulateData = new XMFLOAT4[m_width * m_height];
	ClearAccumulationData();

	auto resourceManager = ResourceManager::Get();
	// srv and uav allocation.
	m_srvAllocation = resourceManager->m_cbvSrvUavAllocator.Allocate(1);
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	m_device->CreateShaderResourceView(m_gpuResource.Get(), &srvDesc, m_srvAllocation.CpuHandle);

	if (m_flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		m_uavAllocation = resourceManager->m_cbvSrvUavAllocator.Allocate(1);
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = texDesc.Format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		m_device->CreateUnorderedAccessView(m_gpuResource.Get(), nullptr, &uavDesc, m_uavAllocation.CpuHandle);
	}
	return S_OK;
}

void Image::Resize(UINT newWidth, UINT newHeight)
{
	if (newWidth == m_width && newHeight == m_height)
		return; // No need to recreate

	// Release old resources
	m_gpuResource.Reset();
	m_uploadBuffer.Reset();

	if (m_pixelData)
	{
		delete[] m_pixelData;
		m_pixelData = nullptr;
	}

	// Recreate the texture with new size
	Initialize(m_device, m_cmdList, m_allocator, newWidth, newHeight, m_format, m_flags);
}

void Image::ClearAccumulationData()
{
	if (m_accumulateData)
	{
		memset(m_accumulateData, 0, m_width * m_height * sizeof(XMFLOAT4));
	}
}

void Image::CommitChanges()
{
	uint8_t* pData;
	m_uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pData));

	// Copy data from our CPU buffer to the upload buffer, respecting the row pitch
	for (UINT y = 0; y < m_height; ++y)
	{
		memcpy(
			pData + y * m_footprint.Footprint.RowPitch,
			m_pixelData + y * m_width,
			m_width * sizeof(uint32_t)
		);
	}
	m_uploadBuffer->Unmap(0, nullptr);

	// Transition the destination resource to be ready for the copy
	TransitionResource(m_cmdList, m_gpuResource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

	D3D12_TEXTURE_COPY_LOCATION dst = {};
	dst.pResource = m_gpuResource.Get();
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.pResource = m_uploadBuffer.Get();
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src.PlacedFootprint = m_footprint;

	m_cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

	// Transition the resource to a shader-readable state
	TransitionResource(m_cmdList, m_gpuResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
