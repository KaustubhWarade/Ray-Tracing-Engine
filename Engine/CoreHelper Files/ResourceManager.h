#pragma once

#include "../RenderEngine Files/global.h"
#include "DescriptorAllocator.h"

class RenderEngine;
struct Model;
class Image;
struct Texture;

#include "Model Loader/ModelLoader.h"
#include "Image.h"
#include "Texture.h"
#include "GpuBuffer.h"

class ResourceManager
{
public:

    ResourceManager();
    ~ResourceManager();

    static ResourceManager* Get();

    HRESULT Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, RenderEngine* renderEngine);
    void Shutdown();
    void FlushUploadBuffers();

    void StoreModel(const std::string& name, std::unique_ptr<Model> model);

    // Asset Loading & Retrieval
    HRESULT LoadModel(const std::string& name, const std::string& filename);
    HRESULT CreateModel(const std::string& name, const void* vertexData, UINT vertexDataSize, UINT vertexStride, const void* indexData, UINT indexDataSize, DXGI_FORMAT indexFormat);
    Model* GetModel(const std::string& name);
    void UnloadModel(const std::string& name);

    HRESULT LoadTextureFromFile(const std::string& name, const std::wstring& filename); // For DDS
    Texture* GetTexture(const std::string& name);
    void UnloadTexture(const std::string& name);

    Texture* CreateTextureFromMemory(const std::string& name, int width, int height, DXGI_FORMAT format, int bytesPerPixel, const unsigned char* pixelData);

    // Potentially for Image class usage directly (e.g., for RT output)
    HRESULT CreateImage(const std::string& name, UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    HRESULT ResizeImage(const std::string& name, UINT width, UINT height);
    Image* GetImage(const std::string& name);
    void UnloadImage(const std::string& name);
    void UpdateImage(const std::string& name, const uint32_t* data);

    // Get a descriptor handle for an asset's SRV/CBV/UAV
    ID3D12DescriptorHeap* GetMainCbvSrvUavHeap() const { return m_mainCbvSrvUavHeap; }

    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuDescriptorHandle(const std::string& assetName);

    ID3D12Device* GetDevice() { return m_device; }
    ID3D12GraphicsCommandList* GetCommandList() { return m_cmdList; }

    DescriptorAllocator m_bindlessTextureAllocator;
    DescriptorAllocator m_generalPurposeHeapAllocator;

    DescriptorHandle m_imguiFontSrvHandle;

    ID3D12Device* m_device;
    ID3D12GraphicsCommandList* m_cmdList;
    RenderEngine* m_renderEngine;
    // Caches for loaded assets
private: 
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    ID3D12DescriptorHeap* m_mainCbvSrvUavHeap;

    std::unordered_map<std::string, std::unique_ptr<Model>> m_models;
    std::unordered_map<std::string, std::unique_ptr<Material>> m_material;
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
    std::unordered_map<std::string, std::unique_ptr<Image>> m_images;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_uploadBuffersToRelease;

    ModelLoader m_modelLoader; 
};