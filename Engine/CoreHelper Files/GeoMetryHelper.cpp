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

	EXECUTE_AND_LOG_RETURN(CreateDefaultBuffer(
		device.Get(),
		cmdList.Get(),
		initData,
		dataSize,
		Mesh.VertexBufferUploader,
		Mesh.VertexBufferGPU));

	Mesh.VertexBufferGPU->SetName((Mesh.name + L" VertexBuffer").c_str());

	Mesh.VertexCount = dataSize / sizeof(ModelVertex);
	Mesh.VertexByteStride = sizeof(ModelVertex);
	Mesh.VertexBufferByteSize = dataSize;

	return S_OK;
}

HRESULT createGeometryIndexResource(Microsoft::WRL::ComPtr<ID3D12Device> device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, GeometryMesh& Mesh, const void* initData, UINT dataSize, DXGI_FORMAT indexFormat)
{
	EXECUTE_AND_LOG_RETURN(CreateDefaultBuffer(
		device.Get(),
		cmdList.Get(),
		initData,
		dataSize,
		Mesh.IndexBufferUploader,
		Mesh.IndexBufferGPU));

	Mesh.IndexBufferGPU->SetName((Mesh.name + L" IndexBufferGPU").c_str());

	Mesh.IndexFormat = indexFormat;
	Mesh.IndexBufferByteSize = dataSize;
	Mesh.IndexCount = dataSize / (indexFormat == DXGI_FORMAT_R16_UINT ? sizeof(uint16_t) : sizeof(uint32_t));
	TransitionResource(
		cmdList.Get(),
		Mesh.IndexBufferGPU.Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_STATE_INDEX_BUFFER);

	return S_OK;
}

