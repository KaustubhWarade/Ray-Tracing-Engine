#include "Image.h"
#include "CommonFunction.h"
#include "extraPackages/d3dx12.h"
#include "ResourceManager.h"
#include "../RenderEngine Files/RenderEngine.h"


Image::Image() {}

Image::~Image()
{
	Shutdown();
}

void Image::Shutdown()
{
	Internal_FreeViews();

	delete[] m_pixelData;
	m_pixelData = nullptr;
	delete[] m_accumulateData;
	m_accumulateData = nullptr;

	m_gpuResource.Reset();
	m_uploadBuffer.Reset();
	m_rtvHeap.Reset();

	m_device = nullptr;
	m_cmdList = nullptr;
	m_allocator = nullptr;
}

HRESULT Image::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, DescriptorAllocator* allocator,
	UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags)
{
	m_device = device;
	m_cmdList = cmdList;
	m_allocator = allocator;
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

	D3D12_HEAP_PROPERTIES heapProperties;
	CreateHeapProperties(heapProperties, D3D12_HEAP_TYPE_DEFAULT);
	EXECUTE_AND_LOG_RETURN(m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&m_gpuResource)));
	m_gpuResource->SetName(L"Image GPU Resource");

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
	m_uploadBuffer->SetName(L"Image Upload Resource");

	if (m_pixelData) delete[] m_pixelData;
	m_pixelData = new uint32_t[width * height];
	ZeroMemory(m_pixelData, width * height * sizeof(uint32_t));
	m_accumulateData = new XMFLOAT4[m_width * m_height];
	ClearAccumulationData();

	// If the image can be a render target, create an RTV for it.
	if (m_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		// Create a dedicated RTV heap for this image
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = 1;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		EXECUTE_AND_LOG_RETURN(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
		m_device->CreateRenderTargetView(m_gpuResource.Get(), nullptr, m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		m_rtvHandle.CpuHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	}

	Internal_CreateViews();
	return S_OK;
}

void Image::Resize(UINT newWidth, UINT newHeight)
{
	if (newWidth == m_width && newHeight == m_height)
		return; // No need to recreate

	// Release old resources
	Internal_FreeViews();
	m_gpuResource.Reset();
	m_uploadBuffer.Reset();
	m_rtvHeap.Reset();
	delete[] m_pixelData;
	delete[] m_accumulateData;

	m_width = newWidth;
	m_height = newHeight;

	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Width = m_width;
	texDesc.Height = m_height;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = m_format;
	texDesc.SampleDesc.Count = 1;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = m_flags;

	D3D12_HEAP_PROPERTIES heapProperties;
	CreateHeapProperties(heapProperties, D3D12_HEAP_TYPE_DEFAULT);
	m_device->CreateCommittedResource(
		&heapProperties, D3D12_HEAP_FLAG_NONE, &texDesc,
		D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_gpuResource));

	m_pixelData = new uint32_t[m_width * m_height];
	ZeroMemory(m_pixelData, m_width * m_height * sizeof(uint32_t));
	m_accumulateData = new XMFLOAT4[m_width * m_height];
	ClearAccumulationData();

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
	m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&uploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_uploadBuffer));

	// Re-create the RTV if needed
	if (m_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = 1;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));

		m_device->CreateRenderTargetView(m_gpuResource.Get(), nullptr, m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		m_rtvHandle.CpuHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	}

	Internal_CreateViews();
}


void Image::Internal_CreateViews()
{
	// Allocate and create the SRV
	m_srvHandle = m_allocator->AllocateSingle();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = m_format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	m_device->CreateShaderResourceView(m_gpuResource.Get(), &srvDesc, m_srvHandle.CpuHandle);

	// If the resource allows UAV, allocate and create it
	if (m_flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		m_uavHandle = m_allocator->AllocateSingle();
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = m_format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		m_device->CreateUnorderedAccessView(m_gpuResource.Get(), nullptr, &uavDesc, m_uavHandle.CpuHandle);
	}
}

void Image::Internal_FreeViews()
{
	if (m_allocator)
	{
		if (m_srvHandle.IsValid())
		{
			m_allocator->FreeSingle(m_srvHandle);
		}
		if (m_uavHandle.IsValid())
		{
			m_allocator->FreeSingle(m_uavHandle);
		}
	}
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
	RenderEngine::Get()->TransitionResource(m_cmdList, m_gpuResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST);

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
	RenderEngine::Get()->TransitionResource(m_cmdList, m_gpuResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
