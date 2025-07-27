#pragma once

#include "../RenderEngine Files/global.h"
using namespace DirectX;

struct ModelVertex {
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexCoord;
};

struct GeometryMesh
{
	std::wstring name;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	UINT VertexCount = 0;
	UINT VertexByteStride = 0;
	UINT VertexBufferByteSize = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> TexCoordBufferUploader = nullptr; 
	Microsoft::WRL::ComPtr<ID3D12Resource> TexCoordBufferGPU = nullptr;
	UINT TexCoordByteStride = 0;
	UINT TexCoordBufferByteSize = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;
	UINT IndexCount = 0;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
	UINT IndexBufferByteSize = 0;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView()
	{
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
		vertexBufferView.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		vertexBufferView.StrideInBytes = VertexByteStride;
		vertexBufferView.SizeInBytes = VertexBufferByteSize;

		return vertexBufferView;
	}

	D3D12_VERTEX_BUFFER_VIEW TexCoordBufferView()
	{
		D3D12_VERTEX_BUFFER_VIEW texCoordBufferView;
		texCoordBufferView.BufferLocation = TexCoordBufferGPU->GetGPUVirtualAddress();
		texCoordBufferView.StrideInBytes = TexCoordByteStride;
		texCoordBufferView.SizeInBytes = TexCoordBufferByteSize;

		return texCoordBufferView;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
	{
		D3D12_INDEX_BUFFER_VIEW indexBufferView;
		indexBufferView.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		indexBufferView.Format = IndexFormat;
		indexBufferView.SizeInBytes = IndexBufferByteSize;

		return indexBufferView;
	}
};

HRESULT createGeometryVertexResource(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList,
	GeometryMesh &Mesh,
	const void *initData,
	UINT dataSize);

HRESULT createGeometryTexCoordResource(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList,
	GeometryMesh &Mesh,
	const void *initData,
	UINT dataSize);

HRESULT createGeometryIndexResource(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList,
	GeometryMesh &Mesh,
	const void *initData,
	UINT dataSize,
	DXGI_FORMAT indexFormat);
