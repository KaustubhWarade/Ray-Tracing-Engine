#pragma once

#include "../../RenderEngine Files/global.h"
#include "../GeoMetryHelper.h"
#include "../Texture.h"
#include "../Mesh.h"
#include "../RayTracingStructs.h"
#include <map>

using BLASHandle = size_t;

class ResourceManager;

namespace tinygltf {
    class Model;
}

// Represents a primitive (a part of a mesh with a specific material)
struct ModelPrimitive {
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    UINT BaseVertexLocation = 0;
    int MaterialIndex = -1;
};

struct ModelMesh {
    std::wstring Name;     

    // --- CPU Data ---
    std::vector<ModelVertex> Vertices;
    std::vector<uint32_t> Indices;
    std::vector<ModelPrimitive> Primitives;

    // --- GPU Data (now directly part of the mesh) ---
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr; // For cleanup
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
    UINT VertexBufferByteSize = 0;
    UINT VertexByteStride = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr; // For cleanup
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;
    UINT IndexBufferByteSize = 0;
    DXGI_FORMAT IndexFormat = DXGI_FORMAT_R32_UINT;

    // Helper functions
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView()
    {
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
        vertexBufferView.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
        vertexBufferView.StrideInBytes = VertexByteStride;
        vertexBufferView.SizeInBytes = VertexBufferByteSize;
        return vertexBufferView;
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

enum LightType { Directional, Point, Spot };

struct ModelLight {
    std::string name;
    LightType type;
    XMFLOAT3 color{ 1.0f, 1.0f, 1.0f };
    float intensity = 1.0f;
    float range = 0.0f; // Infinite
    float innerConeAngle = 0.0f;
    float outerConeAngle = 0.785398f; // PI / 4
};

struct Model
{
    std::vector<ModelMesh> Meshes;
    std::vector<Material> Materials;
    std::vector<ModelLight> Lights;

    std::vector<std::string> MaterialNames;
    std::map<std::string, int> MaterialNameMap;

    void EnsureDefaultMaterial()
    {
        if (Materials.empty())
        {
            Material defaultMat{};
            defaultMat.BaseColorFactor = { 0.8f, 0.8f, 0.8f, 1.0f };
            defaultMat.MetallicFactor = 0.1f;
            defaultMat.RoughnessFactor = 0.8f;
            Materials.push_back(defaultMat);
        }
    }
};

struct ModelInstance {
    std::string Name = "";
    Model* SourceModel = nullptr;

    // Transformation for this specific instance
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 Rotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 Scale = { 1.0f, 1.0f, 1.0f };
    DirectX::XMMATRIX Transform = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX InverseTransform = DirectX::XMMatrixIdentity();

    uint32_t MaterialOffset = 0;
};

struct ModelInstanceGPUData
{
    DirectX::XMMATRIX Transform;
    DirectX::XMMATRIX InverseTransform;
    uint32_t BaseTriangleIndex;
    uint32_t BaseNodeIndex;
    uint32_t MaterialOffset;
    float _padding0;
};

class ModelLoader {
public:
    ModelLoader() = default;;
    ~ModelLoader() = default;;

    HRESULT LoadGLTF(ResourceManager* resourceManager, const std::string& filename, Model& outModel);

private:
    Texture* LoadTexture(
        const tinygltf::Model& gltfModel,
        int textureIndex,
        bool isSrgb,
        const std::filesystem::path& basePath,
        ResourceManager* resourceManager
    );

};