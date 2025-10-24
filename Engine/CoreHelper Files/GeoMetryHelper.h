#pragma once

#include "../RenderEngine Files/global.h"
using namespace DirectX;

struct ModelMesh;

struct ModelVertex {
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexCoord;
	DirectX::XMFLOAT4 Tangent;
};

HRESULT createGeometryVertexResource(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList,
	ModelMesh& mesh,
	const void *initData,
	UINT dataSize);

HRESULT createGeometryIndexResource(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList,
	ModelMesh& mesh,
	const void *initData,
	UINT dataSize,
	DXGI_FORMAT indexFormat);

void CreateSphereMesh(float radius, int sliceCount, int stackCount);

void CreateQuadMesh(float size);
