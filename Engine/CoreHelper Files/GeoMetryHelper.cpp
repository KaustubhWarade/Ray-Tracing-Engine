#include "GeoMetryHelper.h"
#include "../RenderEngine Files/global.h"
#include "CommonFunction.h"

HRESULT createGeometryVertexResource(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList,
	GeometryMesh &Mesh,
	const void *initData,
	UINT dataSize)
{
	D3D12_HEAP_PROPERTIES heapUploadProps = {};
	CreateHeapProperties(heapUploadProps, D3D12_HEAP_TYPE_UPLOAD);

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

	EXECUTE_AND_LOG_RETURN(device->CreateCommittedResource(
		&heapUploadProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&Mesh.VertexBufferUploader)));
	Mesh.VertexBufferUploader->SetName((Mesh.name + L"Vertex").c_str());

	D3D12_HEAP_PROPERTIES heapDefaultProps = {};
	CreateHeapProperties(heapDefaultProps, D3D12_HEAP_TYPE_DEFAULT);

	EXECUTE_AND_LOG_RETURN(device->CreateCommittedResource(
		&heapDefaultProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&Mesh.VertexBufferGPU)));
	Mesh.VertexBufferGPU->SetName((Mesh.name + L"Vertex").c_str());

	void *mapped = nullptr;
	D3D12_RANGE readRange = {0, 0}; // We won�t read after mapping
	Mesh.VertexBufferUploader->Map(0, &readRange, &mapped);
	memcpy(mapped, initData, static_cast<size_t>(dataSize));
	Mesh.VertexBufferUploader->Unmap(0, nullptr);

	TransitionResource(cmdList.Get(), Mesh.VertexBufferGPU.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

	cmdList->CopyBufferRegion(Mesh.VertexBufferGPU.Get(), 0, Mesh.VertexBufferUploader.Get(), 0, dataSize);

	TransitionResource(cmdList.Get(), Mesh.VertexBufferGPU.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	return S_OK;
}

HRESULT createGeometryTexCoordResource(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList,
	GeometryMesh &Mesh,
	const void *initData,
	UINT dataSize)
{
	D3D12_HEAP_PROPERTIES heapUploadProps = {};
	CreateHeapProperties(heapUploadProps, D3D12_HEAP_TYPE_UPLOAD);

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

	EXECUTE_AND_LOG_RETURN(device->CreateCommittedResource(
		&heapUploadProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&Mesh.TexCoordBufferUploader)));
	Mesh.TexCoordBufferUploader->SetName((Mesh.name + L"Texture").c_str());

	D3D12_HEAP_PROPERTIES heapDefaultProps = {};
	CreateHeapProperties(heapDefaultProps, D3D12_HEAP_TYPE_DEFAULT);

	EXECUTE_AND_LOG_RETURN(device->CreateCommittedResource(
		&heapDefaultProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&Mesh.TexCoordBufferGPU)));
	Mesh.TexCoordBufferGPU->SetName((Mesh.name+L"Texture").c_str());

	void *mapped = nullptr;
	D3D12_RANGE readRange = {0, 0};
	Mesh.TexCoordBufferUploader->Map(0, &readRange, &mapped);
	memcpy(mapped, initData, static_cast<size_t>(dataSize));
	Mesh.TexCoordBufferUploader->Unmap(0, nullptr);

	cmdList->CopyBufferRegion(Mesh.TexCoordBufferGPU.Get(), 0, Mesh.TexCoordBufferUploader.Get(), 0, dataSize);

	return S_OK;
}
