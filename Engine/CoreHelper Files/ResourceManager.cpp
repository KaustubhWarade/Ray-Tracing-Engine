#include "ResourceManager.h"

#include "CommonFunction.h" 
#include "../RenderEngine Files/RenderEngine.h" 
#include "extraPackages/d3dx12.h"
#include "thirdParty/stb_image.h" 


// Static instance for the singleton
static std::unique_ptr<ResourceManager> s_instance;

ResourceManager* ResourceManager::Get()
{
    if (!s_instance) {
        s_instance = std::unique_ptr<ResourceManager>(new ResourceManager());
    }
    return s_instance.get();
}

ResourceManager::ResourceManager()
    : m_device(nullptr), m_cmdList(nullptr), m_renderEngine(nullptr)
{
}

ResourceManager::~ResourceManager()
{
    Shutdown();
}

HRESULT ResourceManager::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, RenderEngine* renderEngine)
{
    m_device = device;
    m_cmdList = cmdList;
    m_renderEngine = renderEngine;
    EXECUTE_AND_LOG_RETURN(m_cbvSrvUavAllocator.Create(m_device));
    m_cbvSrvUavAllocator.GetHeap()->SetName(L"ResourceManager Main CBV/SRV/UAV Heap");

    return S_OK;
}

void ResourceManager::Shutdown()
{
    FlushUploadBuffers();
    m_models.clear();
    m_textures.clear();
    m_images.clear();
    m_assetDescriptorAllocations.clear();

}

void ResourceManager::FlushUploadBuffers()
{
    // This should be called after the GPU has finished executing the commands
    // that used these upload buffers. A good place is after a fence wait.
    m_uploadBuffersToRelease.clear();
}

HRESULT ResourceManager::LoadModel(const std::string& name, const std::string& filename)
{
    if (m_models.count(name)) {
#ifdef _DEBUG
            fprintf(gpFile, "Model '%s' already loaded.\n", name.c_str());
#endif
        return S_OK; // Already loaded
    }

    auto model = std::make_unique<Model>();
    HRESULT hr = m_modelLoader.LoadGLTF(Get(), filename, *model);
    if (FAILED(hr)) {
#ifdef _DEBUG
        fprintf(gpFile, "Failed to load glTF model '%s' from '%s'.\n", name.c_str(), filename.c_str());
#endif
        return hr;
    }

    m_models[name] = std::move(model);
#ifdef _DEBUG
    fprintf(gpFile, "Successfully loaded model '%s'.\n", name.c_str());
#endif
    return S_OK;
}

HRESULT ResourceManager::CreateModel(const std::string& name, const void* vertexData, UINT vertexDataSize, UINT vertexStride, const void* indexData, UINT indexDataSize, DXGI_FORMAT indexFormat)
{
    if (m_models.count(name)) {
#ifdef _DEBUG
        fprintf(gpFile, "Model '%s' already exists.\n", name.c_str());
#endif
        return S_OK; // Already exists
    }

    auto model = std::make_unique<Model>();
    ModelMesh mesh = {}; 

    EXECUTE_AND_LOG_RETURN(createGeometryVertexResource(
        m_device, m_cmdList, mesh.GPUData, vertexData, vertexDataSize));

    mesh.GPUData.VertexByteStride = sizeof(ModelVertex);

    if (indexData && indexDataSize > 0)
    {
        EXECUTE_AND_LOG_RETURN(createGeometryIndexResource(
            m_device, m_cmdList, mesh.GPUData, indexData, indexDataSize, indexFormat));
    }

    // A simple procedural mesh has one primitive that covers all indices/vertices
    ModelPrimitive primitive = {};
    primitive.IndexCount = mesh.GPUData.IndexCount;
    primitive.StartIndexLocation = 0;
    primitive.BaseVertexLocation = 0;
    primitive.MaterialIndex = -1; // No material by default
    mesh.Primitives.push_back(primitive);

    model->Meshes.push_back(std::move(mesh));

    m_models[name] = std::move(model);
#ifdef _DEBUG
    fprintf(gpFile, "Successfully created procedural model '%s'.\n", name.c_str());
#endif
    return S_OK;
}

Model* ResourceManager::GetModel(const std::string& name)
{
    auto it = m_models.find(name);
    if (it == m_models.end()) {
#ifdef _DEBUG
        fprintf(gpFile, "Model '%s' not found.\n", name.c_str());
#endif
        return nullptr;
    }
    return it->second.get();
}

void ResourceManager::UnloadModel(const std::string& name)
{
    if (m_models.count(name)) {
        m_models.erase(name); // unique_ptr releases resources
        fprintf(gpFile, "Model '%s' unloaded.\n", name.c_str());
    }
}

HRESULT LoadTextureWithStb(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const wchar_t* filename, ComPtr<ID3D12Resource>& textureResource, ComPtr<ID3D12Resource>& uploadBuffer)
{
    //FILE* pFile = nullptr;
    //// Use _wfopen_s for wide-character filenames on Windows
    //errno_t err = _wfopen_s(&pFile, filename, L"rb");

    //if (err != 0 || pFile == nullptr)
    //{
    //    if (gpFile) fprintf(gpFile, "Failed to open file: %ls\n", filename);
    //    return E_FAIL;
    //}

    //int texWidth, texHeight, texChannels;
    //// Load from the valid FILE* handle
    //stbi_uc* pixels = stbi_load_from_file(pFile, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    //fclose(pFile);
    //if (pixels == nullptr)
    //{
    //    fprintf(gpFile, "STB Failed to load image: %ls. Reason: %s\n", filename, stbi_failure_reason());
    //    return E_FAIL;
    //}

    //D3D12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, texWidth, texHeight, 1, 1);

    //D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    //EXECUTE_AND_LOG_RETURN(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&textureResource)));

    //UINT64 uploadBufferSize;
    //D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
    //device->GetCopyableFootprints(&texDesc, 0, 1, 0, &layout, nullptr, nullptr, &uploadBufferSize);

    //heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    //D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    //EXECUTE_AND_LOG_RETURN(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer)));

    //D3D12_SUBRESOURCE_DATA textureData = {};
    //textureData.pData = pixels;
    //textureData.RowPitch = texWidth * STBI_rgb_alpha;
    //textureData.SlicePitch = textureData.RowPitch * texHeight;

    //UpdateSubresources(cmdList, textureResource.Get(), uploadBuffer.Get(), 0, 0, 1, &textureData);

    //stbi_image_free(pixels); 

    return S_OK;
}

HRESULT ResourceManager::LoadTextureFromFile(const std::string& name, const std::wstring& filename)
{
    if (m_textures.count(name)) {
        fprintf(gpFile, "Texture '%s' already loaded.\n", name.c_str());
        return S_OK;
    }

    auto texture = std::make_unique<Texture>();
    texture->FilePath = filename;

    std::unique_ptr<uint8_t[]> ddsData;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    ComPtr<ID3D12Resource> uploader;

    std::wstring ext = filename.substr(filename.find_last_of(L".") + 1);
    if (ext == L"dds")
    {
        std::unique_ptr<uint8_t[]> ddsData;
        std::vector<D3D12_SUBRESOURCE_DATA> subresources;
        HRESULT hr = DirectX::LoadDDSTextureFromFile(
            m_device,
            filename.c_str(),
            texture->m_ResourceGPU.GetAddressOf(),
            ddsData,
            subresources);

        if (FAILED(hr) || !texture->m_ResourceGPU || subresources.empty())
        {
            fprintf(gpFile, "LoadDDSTextureFromFile() Failed. HRESULT: 0x%08X\n", hr);
            return hr;
        }
        else
        {
            D3D12_RESOURCE_DESC desc = texture->m_ResourceGPU->GetDesc();
            fprintf(gpFile, "Format: %d, Width: %llu, Height: %u, MipLevels: %d, Dimension: %d\n",
                desc.Format, desc.Width, desc.Height, desc.MipLevels, desc.Dimension);
        }

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture->m_ResourceGPU.Get(), 0, static_cast<UINT>(subresources.size()));

        D3D12_HEAP_PROPERTIES heapProps = {};
        CreateHeapProperties(heapProps, D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RESOURCE_DESC bufferDesc = {};
        ZeroMemory(&bufferDesc, sizeof(D3D12_RESOURCE_DESC));
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Alignment = 0;
        bufferDesc.Width = uploadBufferSize;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.SampleDesc.Quality = 0;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        EXECUTE_AND_LOG_RETURN(m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploader)
        ));

        UpdateSubresources(m_cmdList, texture->m_ResourceGPU.Get(), uploader.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());

    }
    else
    {
        // Use our new STB helper for other formats
        EXECUTE_AND_LOG_RETURN(LoadTextureWithStb(m_device, m_cmdList, filename.c_str(), texture->m_ResourceGPU, uploader));
    }

    TransitionResource(
        m_cmdList,
        texture->m_ResourceGPU.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

    texture->SrvAllocation = m_cbvSrvUavAllocator.Allocate(1);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = texture->m_ResourceGPU->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = texture->m_ResourceGPU->GetDesc().MipLevels;
    m_device->CreateShaderResourceView(texture->m_ResourceGPU.Get(), &srvDesc, texture->SrvAllocation.CpuHandle);

    m_textures[name] = std::move(texture);
    m_uploadBuffersToRelease.push_back(uploader);

    return S_OK;
}

Texture* ResourceManager::GetTexture(const std::string& name)
{
    auto it = m_textures.find(name);
    if (it == m_textures.end()) {
        fprintf(gpFile, "Texture '%s' not found.\n", name.c_str());
        return nullptr;
    }
    return it->second.get();
}

void ResourceManager::UnloadTexture(const std::string& name)
{
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        //m_cbvSrvUavAllocator.Free(it->second->SrvAllocation.CpuHandle, it->second->SrvAllocation.GpuHandle);
        m_textures.erase(it);
        fprintf(gpFile, "Texture '%s' unloaded.\n", name.c_str());
    }
}

HRESULT ResourceManager::CreateImage(const std::string& name, UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags)
{
    if (m_images.count(name)) {
        fprintf(gpFile, "Image '%s' already exists.\n", name.c_str());
        m_images[name]->Resize(width, height);
        return S_OK;
    }

    auto image = std::make_unique<Image>();
    HRESULT hr = image->Initialize(m_device, m_cmdList, &m_cbvSrvUavAllocator, width, height, format, flags);
    if (FAILED(hr))
    {
        fprintf(gpFile, "Failed to create Image resource '%s'.\n", name.c_str());
        return hr;
    }

    m_images[name] = std::move(image);
    fprintf(gpFile, "Successfully created Image '%s'.\n", name.c_str());
    return S_OK;
}

Image* ResourceManager::GetImage(const std::string& name)
{
    auto it = m_images.find(name);
    if (it == m_images.end()) {
        fprintf(gpFile, "Image '%s' not found.\n", name.c_str());
        return nullptr;
    }
    return it->second.get();
}

void ResourceManager::UpdateImage(const std::string& name, const uint32_t* data)
{
    Image* img = GetImage(name);
    if (img) {
        //img->SetData(data);
    }
}

void ResourceManager::UnloadImage(const std::string& name)
{
    auto it = m_images.find(name);
    if (it != m_images.end()) {
        m_images.erase(it); // unique_ptr releases resources
        fprintf(gpFile, "Image '%s' unloaded.\n", name.c_str());
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE ResourceManager::GetGpuDescriptorHandle(const std::string& assetName)
{
    auto it = m_assetDescriptorAllocations.find(assetName);
    if (it != m_assetDescriptorAllocations.end()) {
        return it->second.GpuHandle;
    }
    // Fallback if not explicitly allocated and mapped
    // Check if it's a texture from LoadTexture
    auto tex_it = m_textures.find(assetName);
    if (tex_it != m_textures.end()) {
        return tex_it->second->SrvAllocation.GpuHandle;
    }
    // Check if it's an image from CreateImage (if SRV was allocated)
    auto img_it = m_images.find(assetName);
    if (img_it != m_images.end()) {
        auto img_alloc_it = m_assetDescriptorAllocations.find(assetName);
        if (img_alloc_it != m_assetDescriptorAllocations.end()) {
            return img_alloc_it->second.GpuHandle;
        }
    }
    return D3D12_GPU_DESCRIPTOR_HANDLE{}; // Return invalid handle if not found
}

D3D12_GPU_DESCRIPTOR_HANDLE ResourceManager::GetGpuDescriptorHandle(int textureIndex)
{
    if (textureIndex < 0 || textureIndex >= m_textures.size()) { // This assumes m_textures is indexed by a simple int, which it isn't in a map.
        // This function signature is problematic with std::unordered_map.
        // You'd need to map glTF's integer texture index to your string names or a flat vector if you truly want to use this.
        // A better approach is to store the descriptor allocation directly in ModelMaterial or LoadedTexture and access it.
        fprintf(gpFile, "Warning: GetGpuDescriptorHandle by int index is not supported with unordered_map. Use name.\n");
        return D3D12_GPU_DESCRIPTOR_HANDLE{};
    }
    // To make this work, you'd need a std::vector<std::string> of texture names, or a direct std::vector<LoadedTexture*>
    // Or simpler, just access the SrvAllocation directly from the ModelMaterial
    return D3D12_GPU_DESCRIPTOR_HANDLE{}; // Placeholder
}