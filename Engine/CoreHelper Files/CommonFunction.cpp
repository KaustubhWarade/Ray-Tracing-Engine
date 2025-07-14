// commonFunctions

#include "CommonFunction.h"
#include "../RenderEngine Files/global.h"


HRESULT CreateDefaultBuffer(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	const void* initData,
	UINT64 dataSize,
	Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer,
	Microsoft::WRL::ComPtr<ID3D12Resource>& defaultBuffer)
{

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = dataSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heapDefaultProps = {};
	CreateHeapProperties(heapDefaultProps, D3D12_HEAP_TYPE_DEFAULT);
	EXECUTE_AND_LOG_RETURN(device->CreateCommittedResource(
		&heapDefaultProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

	D3D12_HEAP_PROPERTIES heapUploadProps = {};
	CreateHeapProperties(heapUploadProps, D3D12_HEAP_TYPE_UPLOAD);
	EXECUTE_AND_LOG_RETURN(device->CreateCommittedResource(
		&heapUploadProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

	// Describe the data we want to copy into the default buffer.
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;
	subResourceData.RowPitch = dataSize;
	subResourceData.SlicePitch = subResourceData.RowPitch;

	// Schedule a copy from the upload buffer to the default buffer.
	TransitionResource(cmdList, defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_COPY_DEST);

	void* pMappedData = nullptr;
	D3D12_RANGE readRange = { 0, 0 };
	EXECUTE_AND_LOG_RETURN(uploadBuffer->Map(0, &readRange, &pMappedData));
	memcpy(pMappedData, subResourceData.pData, subResourceData.RowPitch);
	D3D12_RANGE writtenRange = { 0, dataSize };
	uploadBuffer->Unmap(0, &writtenRange);

	cmdList->CopyBufferRegion(
		defaultBuffer.Get(),
		0,
		uploadBuffer.Get(),
		0,
		dataSize);

	TransitionResource(cmdList, defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_GENERIC_READ);

	return S_OK;
}


void TransitionResource(
	ID3D12GraphicsCommandList* cmdList,
	ID3D12Resource* resource,
	D3D12_RESOURCE_STATES oldState,
	D3D12_RESOURCE_STATES newState)
{
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource;
	barrier.Transition.StateBefore = oldState;
	barrier.Transition.StateAfter = newState;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	// log resource and barrier states
#ifdef _DEBUG
	if (gpFile != NULL)
	{
		fprintf(gpFile, "Stateless Transition: resource %p from state %d to state %d.\n", resource, oldState, newState);
	}
#endif

	cmdList->ResourceBarrier(1, &barrier);
}


void CreateHeapProperties(D3D12_HEAP_PROPERTIES& heap, D3D12_HEAP_TYPE heapType)
{
	ZeroMemory(&heap, sizeof(D3D12_HEAP_PROPERTIES));
	heap.Type = heapType;
	heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap.CreationNodeMask = 1;
	heap.VisibleNodeMask = 1;
}