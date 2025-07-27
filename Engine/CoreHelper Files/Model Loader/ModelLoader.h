#pragma once

#include "../../RenderEngine Files/global.h"
#include "../GeoMetryHelper.h"
#include "../Texture.h"

class ResourceManager;

namespace tinygltf {
    class Model;
    struct Accessor;
    struct Image;
}

//struct ModelVertex {
//    DirectX::XMFLOAT3 Position;
//    DirectX::XMFLOAT3 Normal;
//    DirectX::XMFLOAT2 TexCoord;
//};

// Represents a material, linking to textures and PBR factors
struct ModelMaterial {
    DirectX::XMFLOAT4 BaseColorFactor; 
    int BaseColorTextureIndex = -1;
    // Add other PBR factors/texture indices as needed:
    // float MetallicFactor;
    // float RoughnessFactor;
    // int MetallicRoughnessTextureIndex = -1;
    // int NormalTextureIndex = -1;
    // int OcclusionTextureIndex = -1;
    // int EmissiveTextureIndex = -1;
    // DirectX::XMFLOAT3 EmissiveFactor; // RGB
    D3D12_GPU_DESCRIPTOR_HANDLE BaseColorTextureHandle{};
};

// Represents a primitive (a part of a mesh with a specific material)
struct ModelPrimitive {
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    UINT BaseVertexLocation = 0;
    int MaterialIndex = -1;
};


struct ModelMesh {
    std::wstring Name;      
    GeometryMesh GPUData;   
    std::vector<ModelPrimitive> Primitives;
    std::vector<ModelVertex> Vertices;
    std::vector<uint32_t> Indices;
};

struct Model {
    std::vector<ModelMesh> Meshes;
    std::vector<ModelMaterial> Materials;
    std::vector<Texture*> Textures;
};

class ModelLoader {
public:
    ModelLoader();
    ~ModelLoader(); 

    HRESULT LoadGLTF(ResourceManager* resourceManager, const std::string& filename, Model& outModel);

private:
    HRESULT GetBufferDataFromAccessor(const tinygltf::Model& gltfModel, const tinygltf::Accessor& accessor, const uint8_t** outData, size_t* outDataSize);

};