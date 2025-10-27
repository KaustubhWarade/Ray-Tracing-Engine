#include "GeoMetryHelper.h"
#include "../RenderEngine Files/global.h"
#include "../RenderEngine Files/RenderEngine.h"
#include "CommonFunction.h"
#include "Model Loader/ModelLoader.h" 

HRESULT createGeometryVertexResource(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList,
	ModelMesh &mesh,
	const void *initData,
	UINT dataSize)
{
	EXECUTE_AND_LOG_RETURN(CreateDefaultBuffer(
		device.Get(),
		cmdList.Get(),
		initData,
		dataSize,
		mesh.VertexBufferUploader,
		mesh.VertexBufferGPU));

	mesh.VertexBufferGPU->SetName((mesh.Name + L" VertexBuffer").c_str());

	mesh.VertexByteStride = sizeof(ModelVertex);
	mesh.VertexBufferByteSize = dataSize;

	return S_OK;
}

HRESULT createGeometryIndexResource(
	Microsoft::WRL::ComPtr<ID3D12Device> device, 
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, 
	ModelMesh& mesh, 
	const void* initData, 
	UINT dataSize, 
	DXGI_FORMAT indexFormat)
{
	EXECUTE_AND_LOG_RETURN(CreateDefaultBuffer(
		device.Get(),
		cmdList.Get(),
		initData,
		dataSize,
		mesh.IndexBufferUploader,
		mesh.IndexBufferGPU));

	mesh.IndexBufferGPU->SetName((mesh.Name + L" IndexBufferGPU").c_str());

	mesh.IndexFormat = indexFormat;
	mesh.IndexBufferByteSize = dataSize;
	//mesh.IndexCount = dataSize / (indexFormat == DXGI_FORMAT_R16_UINT ? sizeof(uint16_t) : sizeof(uint32_t));
	RenderEngine::Get()->TransitionResource(
		cmdList.Get(),
		mesh.IndexBufferGPU.Get(),
		D3D12_RESOURCE_STATE_INDEX_BUFFER);

	return S_OK;
}

void CreateSphereMesh(float radius, int sliceCount, int stackCount)
{
	std::vector<ModelVertex> vertices;
	std::vector<uint32_t> indices;
	// Add top vertex
	vertices.push_back({ {0.0f, radius, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} });

	float phiStep = XM_PI / stackCount;
	float thetaStep = 2.0f * XM_PI / sliceCount;

	for (int i = 1; i <= stackCount - 1; ++i) {
		float phi = i * phiStep;
		for (int j = 0; j <= sliceCount; ++j) {
			float theta = j * thetaStep;
			ModelVertex v;
			v.Position.x = radius * sinf(phi) * cosf(theta);
			v.Position.y = radius * cosf(phi);
			v.Position.z = radius * sinf(phi) * sinf(theta);

			XMVECTOR p = XMLoadFloat3(&v.Position);
			XMStoreFloat3(&v.Normal, XMVector3Normalize(p));

			v.TexCoord.x = theta / (2.0f * XM_PI);
			v.TexCoord.y = phi / XM_PI;

			// Basic tangent calculation
			v.Tangent.x = -radius * sinf(phi) * sinf(theta);
			v.Tangent.y = 0;
			v.Tangent.z = radius * sinf(phi) * cosf(theta);
			v.Tangent.w = 1.0f;

			vertices.push_back(v);
		}
	}
	// Add bottom vertex
	vertices.push_back({ {0.0f, -radius, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f} });

	for (int i = 1; i <= sliceCount; ++i) {
		indices.push_back(0);
		indices.push_back(i);
		indices.push_back(i + 1);
	}

	int baseIndex = 1;
	int ringVertexCount = sliceCount + 1;
	for (int i = 0; i < stackCount - 2; ++i) {
		for (int j = 0; j < sliceCount; ++j) {
			indices.push_back(baseIndex + i * ringVertexCount + j);
			indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
			indices.push_back(baseIndex + i * ringVertexCount + j + 1);

			indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
			indices.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
			indices.push_back(baseIndex + i * ringVertexCount + j + 1);
		}
	}

	int southPoleIndex = (int)vertices.size() - 1;
	baseIndex = southPoleIndex - ringVertexCount;
	for (int i = 0; i < sliceCount; ++i) {
		indices.push_back(southPoleIndex);
		indices.push_back(baseIndex + i + 1);
		indices.push_back(baseIndex + i);
	}

	ResourceManager::Get()->CreateModel("material_preview_sphere",
		vertices.data(), (UINT)vertices.size() * sizeof(ModelVertex), sizeof(ModelVertex),
		indices.data(), (UINT)indices.size() * sizeof(uint32_t), DXGI_FORMAT_R32_UINT);
}

void CreateQuadMesh(float size)
{
	float s = size / 2.0f;
	ModelVertex vertices[] = {
		{ {-s, -s, -1}, {0, 0, -1}, {0, 1}, {1, 0, 0, 1} },
		{ {-s,  s, -1}, {0, 0, -1}, {0, 0}, {1, 0, 0, 1} },
		{ { s,  s, -1}, {0, 0, -1}, {1, 0}, {1, 0, 0, 1} },
		{ { s, -s, -1}, {0, 0, -1}, {1, 1}, {1, 0, 0, 1} }
	};
	uint32_t indices[] = { 0, 1, 2, 0, 2, 3 };

	ResourceManager::Get()->CreateModel("material_preview_quad",
		vertices, sizeof(vertices), sizeof(ModelVertex),
		indices, sizeof(indices), DXGI_FORMAT_R32_UINT);
}

