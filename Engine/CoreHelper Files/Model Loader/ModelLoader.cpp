#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE

#include "ModelLoader.h"
#include "../thirdParty/tiny_gltf.h"
#include "../CommonFunction.h"
#include "../ResourceManager.h"
#include <filesystem>
#include "../extraPackages/d3dx12.h"


#define D3DX12_NO_STATE_OBJECT_HELPERS
#define D3DX12_NO_CHECK_FEATURE_SUPPORT_CLASS

ModelLoader::ModelLoader() {

}

ModelLoader::~ModelLoader() {

}

HRESULT ModelLoader::GetBufferDataFromAccessor(const tinygltf::Model& gltfModel, const tinygltf::Accessor& accessor, const uint8_t** outData, size_t* outDataSize)
{
	const auto& bufferView = gltfModel.bufferViews[accessor.bufferView];
	const auto& buffer = gltfModel.buffers[bufferView.buffer];

	*outData = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
	*outDataSize = accessor.count * tinygltf::GetComponentSizeInBytes(accessor.componentType) * tinygltf::GetNumComponentsInType(accessor.type);

	if (*outData == nullptr || *outDataSize == 0)
	{
		return E_FAIL;
	}

	return S_OK;
}
HRESULT ModelLoader::LoadGLTF(ResourceManager* resourceManager, const std::string& filename, Model& outModel)
{
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ret = filename.substr(filename.length() - 4) == ".glb"
        ? loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filename)
        : loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filename);

    if (!warn.empty()) fprintf(gpFile, "glTF WARN: %s\n", warn.c_str());
    if (!err.empty()) { fprintf(gpFile, "glTF ERR: %s\n", err.c_str()); return E_FAIL; }
    if (!ret) return E_FAIL;

    auto device = resourceManager->GetDevice();
    auto cmdList = resourceManager->GetCommandList();

    outModel.Meshes.clear();
    outModel.Materials.clear();
    outModel.Textures.clear();

    // -----------------------------
    // Load Materials
    // -----------------------------
    for (size_t i = 0; i < gltfModel.materials.size(); ++i)
    {
        ModelMaterial mat{};
        const auto& m = gltfModel.materials[i];
        // baseColorFactor
        if (m.pbrMetallicRoughness.baseColorFactor.size() == 4)
        {
            mat.BaseColorFactor = DirectX::XMFLOAT4(
                (float)m.pbrMetallicRoughness.baseColorFactor[0],
                (float)m.pbrMetallicRoughness.baseColorFactor[1],
                (float)m.pbrMetallicRoughness.baseColorFactor[2],
                (float)m.pbrMetallicRoughness.baseColorFactor[3]);
        }
        else
        {
            mat.BaseColorFactor = DirectX::XMFLOAT4(1, 1, 1, 1);
        }

        // baseColorTexture
        if (m.pbrMetallicRoughness.baseColorTexture.index >= 0)
        {
            mat.BaseColorTextureIndex = m.pbrMetallicRoughness.baseColorTexture.index;
        }

        outModel.Materials.push_back(mat);
    }

    // -----------------------------
    // Load Meshes
    // -----------------------------
    for (const auto& gltfMesh : gltfModel.meshes)
    {
        ModelMesh mesh{};
        mesh.Name = std::wstring(gltfMesh.name.begin(), gltfMesh.name.end());

        std::vector<ModelVertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<ModelPrimitive> primitives;

        UINT baseVertex = 0;
        UINT baseIndex = 0;

        for (const auto& prim : gltfMesh.primitives)
        {
            // ---- Position ----
            auto posIt = prim.attributes.find("POSITION");
            if (posIt == prim.attributes.end()) continue;
            const tinygltf::Accessor& posAcc = gltfModel.accessors[posIt->second];
            const tinygltf::BufferView& posView = gltfModel.bufferViews[posAcc.bufferView];
            const tinygltf::Buffer& posBuf = gltfModel.buffers[posView.buffer];
            const float* posData = reinterpret_cast<const float*>(
                posBuf.data.data() + posView.byteOffset + posAcc.byteOffset);

            // ---- Normal ----
            const float* normalData = nullptr;
            auto normIt = prim.attributes.find("NORMAL");
            if (normIt != prim.attributes.end())
            {
                const tinygltf::Accessor& normAcc = gltfModel.accessors[normIt->second];
                const tinygltf::BufferView& normView = gltfModel.bufferViews[normAcc.bufferView];
                const tinygltf::Buffer& normBuf = gltfModel.buffers[normView.buffer];
                normalData = reinterpret_cast<const float*>(
                    normBuf.data.data() + normView.byteOffset + normAcc.byteOffset);
            }

            // ---- TexCoord ----
            const float* texData = nullptr;
            auto uvIt = prim.attributes.find("TEXCOORD_0");
            if (uvIt != prim.attributes.end())
            {
                const tinygltf::Accessor& uvAcc = gltfModel.accessors[uvIt->second];
                const tinygltf::BufferView& uvView = gltfModel.bufferViews[uvAcc.bufferView];
                const tinygltf::Buffer& uvBuf = gltfModel.buffers[uvView.buffer];
                texData = reinterpret_cast<const float*>(
                    uvBuf.data.data() + uvView.byteOffset + uvAcc.byteOffset);
            }

            size_t vertCount = posAcc.count;
            for (size_t v = 0; v < vertCount; ++v)
            {
                ModelVertex mv{};
                mv.Position = XMFLOAT3(
                    posData[v * 3 + 0],
                    posData[v * 3 + 1],
                    posData[v * 3 + 2]);

                if (normalData)
                {
                    mv.Normal = XMFLOAT3(
                        normalData[v * 3 + 0],
                        normalData[v * 3 + 1],
                        normalData[v * 3 + 2]);
                }
                else
                {
                    mv.Normal = XMFLOAT3(0, 1, 0);
                }

                if (texData)
                {
                    mv.TexCoord = XMFLOAT2(
                        texData[v * 2 + 0],
                        texData[v * 2 + 1]);
                }
                else
                {
                    mv.TexCoord = XMFLOAT2(0, 0);
                }

                vertices.push_back(mv);
            }

            // ---- Indices ----
            const tinygltf::Accessor& idxAcc = gltfModel.accessors[prim.indices];
            const tinygltf::BufferView& idxView = gltfModel.bufferViews[idxAcc.bufferView];
            const tinygltf::Buffer& idxBuf = gltfModel.buffers[idxView.buffer];
            const uint8_t* idxData = idxBuf.data.data() + idxView.byteOffset + idxAcc.byteOffset;

            size_t idxCount = idxAcc.count;
            for (size_t i = 0; i < idxCount; ++i)
            {
                uint32_t idx = 0;
                switch (idxAcc.componentType)
                {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    idx = reinterpret_cast<const uint16_t*>(idxData)[i];
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    idx = reinterpret_cast<const uint32_t*>(idxData)[i];
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    idx = reinterpret_cast<const uint8_t*>(idxData)[i];
                    break;
                default:
                    break;
                }
                indices.push_back(idx + baseVertex);
            }

            ModelPrimitive primInfo{};
            primInfo.IndexCount = (UINT)idxCount;
            primInfo.StartIndexLocation = baseIndex;
            primInfo.BaseVertexLocation = 0;
            primInfo.MaterialIndex = prim.material;
            primitives.push_back(primInfo);

            baseVertex += (UINT)vertCount;
            baseIndex += (UINT)idxCount;
        }
        // --- Create the single, combined GPU buffers for the entire mesh ---
        if (!vertices.empty())
        {
            UINT vbByteSize = (UINT)vertices.size() * sizeof(ModelVertex);
            createGeometryVertexResource(device, cmdList, mesh.GPUData, vertices.data(), vbByteSize);
            mesh.Vertices = vertices;
        }

        if (!indices.empty())
        {
            UINT ibByteSize = (UINT)indices.size() * sizeof(uint32_t);
            createGeometryIndexResource(device, cmdList, mesh.GPUData, indices.data(), ibByteSize, DXGI_FORMAT_R32_UINT);
            mesh.Indices = indices;
        }

        mesh.Primitives = primitives;
        outModel.Meshes.push_back(std::move(mesh));
    }

    return S_OK;
}