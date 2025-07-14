#include "ResourceManager.h"
#include "DDSTextureLoader12.h" 
#include "CommonFunction.h"

// Constructor
ResourceManager::ResourceManager(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
    : m_pDevice(device), m_pCmdList(cmdList) {
}

// Destructor
ResourceManager::~ResourceManager() = default;

// Texture Loading
TextureHandle ResourceManager::LoadTexture(const std::wstring& filePath)
{
    // 1. Check if we've already loaded this texture
    auto it = m_TextureRegistry.find(filePath);
    if (it != m_TextureRegistry.end())
    {
        return it->second; // Return existing handle
    }

    // 2. If not, load it from disk
    auto newTexture = std::make_unique<Texture>();
    newTexture->Name = filePath;

    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    ComPtr<ID3D12Resource> uploadHeap; // DDSTextureLoader needs this

    // DirectX::LoadDDSTextureFromFile is a bit awkward. It creates the final resource for us.
    // For simplicity, we'll use it directly. A more advanced system might separate loading pixel data
    // from creating the GPU resource.
    EXECUTE_AND_LOG_RETURN(DirectX::LoadDDSTextureFromFile(
        m_pDevice,
        filePath.c_str(),
        &newTexture->Resource,
        uploadHeap, // We need to keep this uploader alive
        subresources
    ));

    // Keep the uploader alive until the command list is executed
    m_UploadBuffers.push_back(uploadHeap);

    // 3. Create a new handle and store the resource
    TextureHandle handle = m_NextHandle++;
    m_Textures[handle] = std::move(newTexture);
    m_TextureRegistry[filePath] = handle;

    return handle;
}

ID3D12Resource* ResourceManager::GetTexture(TextureHandle handle)
{
    auto it = m_Textures.find(handle);
    if (it != m_Textures.end())
    {
        return it->second->Resource.Get();
    }
    return nullptr;
}

// Mesh Creation (Example for your existing cube)
MeshHandle ResourceManager::CreateCubeMesh()
{
    const std::wstring CUBE_MESH_NAME = L"builtin::cube";

    // Check registry first
    auto it = m_MeshRegistry.find(CUBE_MESH_NAME);
    if (it != m_MeshRegistry.end())
    {
        return it->second;
    }

    auto newMesh = std::make_unique<GeometryMesh>();
    newMesh->name = L"Cube";

    // Your existing cube data
    const Vertex_Pos_Tex cube_position[] = { /* ... your 36 vertices ... */ };

    ComPtr<ID3D12Resource> vertexUploadBuffer;
    // Use a generic buffer creation helper
    CreateDefaultBuffer(
        m_pDevice, m_pCmdList,
        cube_position, sizeof(cube_position),
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        vertexUploadBuffer,
        newMesh->VertexBufferGPU,
        L"CubeVertexBuffer"
    );
    m_UploadBuffers.push_back(vertexUploadBuffer);

    newMesh->VertexByteStride = sizeof(Vertex_Pos_Tex);
    newMesh->VertexBufferByteSize = sizeof(cube_position);
    newMesh->VertexCount = 36;

    // 3. Create handle and store
    MeshHandle handle = m_NextHandle++;
    m_Meshes[handle] = std::move(newMesh);
    m_MeshRegistry[CUBE_MESH_NAME] = handle;

    return handle;
}

GeometryMesh* ResourceManager::GetMesh(MeshHandle handle)
{
    auto it = m_Meshes.find(handle);
    if (it != m_Meshes.end())
    {
        return it->second.get();
    }
    return nullptr;
}