// commonFunctions

#include "CommonFunction.h"
#include "../RenderEngine Files/global.h"
#include "../RenderEngine Files/RenderEngine.h"
#include <shobjidl.h> 
#include <Windows.h>


HRESULT CreateDefaultBuffer(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	const void* initData,
	UINT64 dataSize,
	Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer,
	Microsoft::WRL::ComPtr<ID3D12Resource>& defaultBuffer,
	UINT64 bufferSize)
{
	if (dataSize == 0 && bufferSize == 0)
	{
		uploadBuffer.Reset();
		defaultBuffer.Reset();
		return S_OK;
	}
	UINT64 finalBufferSize = (bufferSize > dataSize) ? bufferSize : dataSize;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = finalBufferSize;
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

	RenderEngine::Get()->TrackResource(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON);

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
	RenderEngine::Get()->TransitionResource(cmdList, defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST);

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

	RenderEngine::Get()->TransitionResource(cmdList, defaultBuffer.Get(), D3D12_RESOURCE_STATE_GENERIC_READ);

	return S_OK;
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

std::wstring OpenFileDialog()
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr))
		return L"";

	IFileOpenDialog* pFileOpen = nullptr;
	hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
		IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

	if (SUCCEEDED(hr))
	{
		hr = pFileOpen->Show(nullptr);
		if (SUCCEEDED(hr))
		{
			IShellItem* pItem;
			hr = pFileOpen->GetResult(&pItem);
			if (SUCCEEDED(hr))
			{
				PWSTR pszFilePath = nullptr;
				hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
				if (SUCCEEDED(hr))
				{
					std::wstring filePath(pszFilePath);
					CoTaskMemFree(pszFilePath);
					pItem->Release();
					pFileOpen->Release();
					CoUninitialize();
					return filePath;
				}
				pItem->Release();
			}
		}
		pFileOpen->Release();
	}
	CoUninitialize();
	return L"";
}

void RenderModel(ID3D12GraphicsCommandList* cmdList, Model* model)
{
	if (!model) return;

	// Iterate over each mesh in the model.
	for (auto& mesh : model->Meshes)
	{
		// Set the vertex and index buffers for this mesh.
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView = mesh.VertexBufferView();
		cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);

		D3D12_INDEX_BUFFER_VIEW indexBufferView = mesh.IndexBufferView();
		cmdList->IASetIndexBuffer(&indexBufferView);

		// Iterate over the primitives (sub-meshes). Each is a separate draw call.
		for (auto& primitive : mesh.Primitives)
		{
			// The current shader doesn't use per-primitive material data,
			// so we just issue the draw call. The material color is ignored.

			cmdList->DrawIndexedInstanced(
				primitive.IndexCount,
				1,
				primitive.StartIndexLocation,
				primitive.BaseVertexLocation,
				0
			);
		}
	}
}

void RenderModel(ID3D12GraphicsCommandList* cmdList, Model* model, UINT materialRootParamIndex)
{
	if (!model) return;

	for (auto& mesh : model->Meshes)
	{
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView = mesh.VertexBufferView();
		cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);

		D3D12_INDEX_BUFFER_VIEW indexBufferView = mesh.IndexBufferView();
		cmdList->IASetIndexBuffer(&indexBufferView);

		for (auto& primitive : mesh.Primitives)
		{
			cmdList->SetGraphicsRoot32BitConstant(materialRootParamIndex, primitive.MaterialIndex, 0);

			cmdList->DrawIndexedInstanced(
				primitive.IndexCount,
				1,
				primitive.StartIndexLocation,
				primitive.BaseVertexLocation,
				0
			);
		}
	}
}
