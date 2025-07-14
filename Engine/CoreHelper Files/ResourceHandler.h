#pragma once
#pragma once

#include "../RenderEngine Files/global.h"
#include "ResourceHandle.h"
#include "GeoMetryHelper.h" 

using TextureHandle = uint32_t;
using MeshHandle = uint32_t;

const uint32_t INVALID_HANDLE = 0;

struct Texture;

class ResourceManager
{
public:
    ResourceManager(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    ~ResourceManager();

    void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);


    TextureHandle LoadTexture(const std::wstring& filePath);
    ID3D12Resource* GetTexture(TextureHandle handle);


    MeshHandle CreateCubeMesh();


    GeometryMesh* GetMesh(MeshHandle handle);

    void UploadInitialResources();

private:
    ID3D12Device* m_pDevice = nullptr;
    ID3D12GraphicsCommandList* m_pCmdList = nullptr; // For uploads

    // Helper for generating unique handles
    uint32_t m_NextHandle = 1;

    // --- Internal Storage ---
    // Map from file path to handle to avoid duplicate loading
    std::map<std::wstring, TextureHandle> m_TextureRegistry;
    std::map<std::wstring, MeshHandle> m_MeshRegistry;

    // Map from handle to the actual resource data
    std::map<TextureHandle, std::unique_ptr<Texture>> m_Textures;
    std::map<MeshHandle, std::unique_ptr<GeometryMesh>> m_Meshes;

    // Upload buffers need to be kept alive until the copy is done on the GPU
    std::vector<ComPtr<ID3D12Resource>> m_UploadBuffers;
};

// Internal struct to hold texture data
struct Texture
{
    std::wstring Name;
    ComPtr<ID3D12Resource> Resource = nullptr;
    // We can add more info here later, like width, height, format, etc.
};