#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include "ModelLoader.h"
#include "../thirdParty/tiny_gltf.h"
#include "../CommonFunction.h"
#include "../ResourceManager.h"
#include <filesystem>
#include "../extraPackages/d3dx12.h"
#include <cmath> 


#define D3DX12_NO_STATE_OBJECT_HELPERS
#define D3DX12_NO_CHECK_FEATURE_SUPPORT_CLASS

Texture* ModelLoader::LoadTexture(
    const tinygltf::Model& gltfModel,
    int textureIndex,
    bool isSrgb,
    const std::filesystem::path& basePath,
    ResourceManager* resourceManager)
{
    if (textureIndex < 0) {
        return nullptr; 
    }

    const auto& gltfTexture = gltfModel.textures[textureIndex];
    if (gltfTexture.source < 0) {
        return nullptr;
    }

    const auto& gltfImage = gltfModel.images[gltfTexture.source];

    std::string textureName;
    if (!gltfImage.uri.empty()) {
        textureName = gltfImage.uri;
    }
    else {
        textureName = basePath.filename().string() + "_texture_" + std::to_string(textureIndex);
    }

    Texture* existingTexture = resourceManager->GetTexture(textureName);
    if (existingTexture) {
        return existingTexture;
    }

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    int bytesPerPixel = 0;

    if (gltfImage.bits == 8) {
        bytesPerPixel = gltfImage.component;
        switch (gltfImage.component) {
        case 1: format = DXGI_FORMAT_R8_UNORM; break;
        case 2: format = DXGI_FORMAT_R8G8_UNORM; break;
        case 3:
        case 4:
            format = isSrgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
            bytesPerPixel = 4;
            break;
        }
    }
    else if (gltfImage.bits == 16) {
        bytesPerPixel = gltfImage.component * 2;
        switch (gltfImage.component) {
        case 1: format = DXGI_FORMAT_R16_UNORM; break;
        case 2: format = DXGI_FORMAT_R16G16_UNORM; break;
        case 3:
        case 4: format = DXGI_FORMAT_R16G16B16A16_UNORM; break;
        }
    }

    if (format == DXGI_FORMAT_UNKNOWN) {
        fprintf(gpFile, "WARN: Unsupported texture format in GLTF. Bits: %d, Components: %d\n", gltfImage.bits, gltfImage.component);
        return nullptr;
    }

    if (!gltfImage.uri.empty()) {
        std::filesystem::path texturePath = basePath / gltfImage.uri;
        HRESULT hr = resourceManager->LoadTextureFromFile(textureName, texturePath.wstring());
        if (SUCCEEDED(hr)) {
            return resourceManager->GetTexture(textureName);
        }
    }
    else {
        return resourceManager->CreateTextureFromMemory(
            textureName, gltfImage.width, gltfImage.height, format,
            bytesPerPixel, gltfImage.image.data());

    }
    return nullptr;
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
    std::filesystem::path basePath = std::filesystem::path(filename).parent_path();

    auto device = resourceManager->GetDevice();
    auto cmdList = resourceManager->GetCommandList();

    outModel.Meshes.clear();
    outModel.Materials.clear();

    // -----------------------------
    // Load Materials
    // -----------------------------
    for (size_t i = 0; i < gltfModel.materials.size(); ++i)
    {
        Material mat{};
        const auto& material = gltfModel.materials[i];

        int materialIndex = static_cast<int>(outModel.Materials.size());
        std::string materialName = material.name.empty() ? ("Material_" + std::to_string(materialIndex)) : material.name;
        outModel.MaterialNames.push_back(materialName);

        outModel.MaterialNameMap[materialName] = materialIndex;

        const auto& pbr = material.pbrMetallicRoughness;
       
        if (pbr.baseColorFactor.size() == 4) {
            mat.BaseColorFactor = XMFLOAT4((float)pbr.baseColorFactor[0], (float)pbr.baseColorFactor[1], (float)pbr.baseColorFactor[2], (float)pbr.baseColorFactor[3]);
        }
        mat.MetallicFactor = (float)pbr.metallicFactor;
        mat.RoughnessFactor = (float)pbr.roughnessFactor;
        if (material.emissiveFactor.size() == 3) {
            mat.EmissiveFactor = XMFLOAT3((float)material.emissiveFactor[0], (float)material.emissiveFactor[1], (float)material.emissiveFactor[2]);
        }

        if (pbr.baseColorTexture.index >= 0) {
            Texture* loadedTexture = LoadTexture(gltfModel, pbr.baseColorTexture.index, true, basePath, resourceManager); // isSrgb = true
            if (loadedTexture) {
                mat.BaseColorTextureIndex = loadedTexture->BindlessSrvIndex;
            }
            
        }
        if (material.normalTexture.index >= 0) {
            Texture* loadedTexture = LoadTexture(gltfModel, material.normalTexture.index, false, basePath, resourceManager); // isSrgb = false
            if (loadedTexture) {
                mat.NormalTextureIndex = loadedTexture->BindlessSrvIndex;
            }
        }
        if (pbr.metallicRoughnessTexture.index >= 0) {
            Texture* loadedTexture = LoadTexture(gltfModel, pbr.metallicRoughnessTexture.index, false, basePath, resourceManager); // isSrgb = false
            if (loadedTexture) {
                mat.MetallicRoughnessTextureIndex = loadedTexture->BindlessSrvIndex;
            }
        }
        if (material.occlusionTexture.index >= 0) {
            Texture* loadedTexture = LoadTexture(gltfModel, material.occlusionTexture.index, false, basePath, resourceManager); // isSrgb = false
            if (loadedTexture) {
                mat.OcclusionTextureIndex = loadedTexture->BindlessSrvIndex;
            }
        }
        if (material.emissiveTexture.index >= 0) {
            Texture* loadedTexture = LoadTexture(gltfModel, material.emissiveTexture.index, true, basePath, resourceManager);
            if (loadedTexture) {
                mat.EmissiveTextureIndex = loadedTexture->BindlessSrvIndex;
            }
        }

        if (material.alphaMode == "BLEND") {
            mat.AlphaMode = 2;
        }
        else if (material.alphaMode == "MASK") {
            mat.AlphaMode = 1;
        }

        mat.AlphaCutoff = (float)material.alphaCutoff;
        mat.DoubleSided = material.doubleSided;

        //KHR_materials_unlit
        if (material.extensions.find("KHR_materials_unlit") != material.extensions.end()) {
            mat.Unlit = true;
        }

        // KHR_materials_clearcoat
        if (material.extensions.find("KHR_materials_clearcoat") != material.extensions.end()) {
            const auto& ext = material.extensions.at("KHR_materials_clearcoat");
            if (ext.Has("clearcoatFactor")) mat.ClearcoatFactor = (float)ext.Get("clearcoatFactor").Get<double>();
            if (ext.Has("clearcoatRoughnessFactor")) mat.ClearcoatRoughnessFactor = (float)ext.Get("clearcoatRoughnessFactor").Get<double>();
            if (ext.Has("clearcoatTexture")) {
                int texIndex = ext.Get("clearcoatTexture").Get("index").Get<int>();
                Texture* loadedTexture = LoadTexture(gltfModel, texIndex, false, basePath, resourceManager); // Linear
                if (loadedTexture) mat.ClearcoatTextureIndex = loadedTexture->BindlessSrvIndex;
            }
            if (ext.Has("clearcoatRoughnessTexture")) {
                int texIndex = ext.Get("clearcoatRoughnessTexture").Get("index").Get<int>();
                Texture* loadedTexture = LoadTexture(gltfModel, texIndex, false, basePath, resourceManager); // Linear
                if (loadedTexture) mat.ClearcoatRoughnessTextureIndex = loadedTexture->BindlessSrvIndex;
            }
            if (ext.Has("clearcoatNormalTexture")) {
                int texIndex = ext.Get("clearcoatNormalTexture").Get("index").Get<int>();
                Texture* loadedTexture = LoadTexture(gltfModel, texIndex, false, basePath, resourceManager); // Linear
                if (loadedTexture) mat.ClearcoatNormalTextureIndex = loadedTexture->BindlessSrvIndex;
            }
        }

        // KHR_materials_sheen
        if (material.extensions.find("KHR_materials_sheen") != material.extensions.end()) {
            const auto& ext = material.extensions.at("KHR_materials_sheen");
            if (ext.Has("sheenColorFactor")) {
                auto factor = ext.Get("sheenColorFactor").Get<tinygltf::Value::Array>();
                mat.SheenColorFactor = XMFLOAT3(
                    (float)factor[0].Get<double>(),
                    (float)factor[1].Get<double>(),
                    (float)factor[2].Get<double>()
                );
            }
            if (ext.Has("sheenRoughnessFactor")) {
                mat.SheenRoughnessFactor = (float)ext.Get("sheenRoughnessFactor").Get<double>();
            }
            if (ext.Has("sheenColorTexture")) {
                int texIndex = ext.Get("sheenColorTexture").Get("index").Get<int>();
                Texture* loadedTexture = LoadTexture(gltfModel, texIndex, true, basePath, resourceManager); // sRGB
                if (loadedTexture) mat.SheenColorTextureIndex = loadedTexture->BindlessSrvIndex;
            }
            if (ext.Has("sheenRoughnessTexture")) {
                int texIndex = ext.Get("sheenRoughnessTexture").Get("index").Get<int>();
                Texture* loadedTexture = LoadTexture(gltfModel, texIndex, false, basePath, resourceManager); // Linear
                if (loadedTexture) mat.SheenRoughnessTextureIndex = loadedTexture->BindlessSrvIndex;
            }
        }

        // KHR_materials_ior
        if (material.extensions.find("KHR_materials_ior") != material.extensions.end()) {
            const auto& ext = material.extensions.at("KHR_materials_ior");
            if (ext.Has("ior")) {
                mat.IOR = (float)ext.Get("ior").Get<double>();
            }
        }

        // KHR_materials_transmission
        if (material.extensions.find("KHR_materials_transmission") != material.extensions.end()) {
            const auto& ext = material.extensions.at("KHR_materials_transmission");
            if (ext.Has("transmissionFactor")) {
                mat.Transmission = (float)ext.Get("transmissionFactor").Get<double>();
            }
            if (ext.Has("transmissionTexture")) {
                int texIndex = ext.Get("transmissionTexture").Get("index").Get<int>();
                Texture* loadedTexture = LoadTexture(gltfModel, texIndex, false, basePath, resourceManager); // Linear
                if (loadedTexture) mat.TransmissionTextureIndex = loadedTexture->BindlessSrvIndex;
            }
        }


        outModel.Materials.push_back(mat);
    }

    outModel.EnsureDefaultMaterial();

    fprintf(gpFile, "\n--- glTF Light Information ---\n");

    // KHR_lights_punctual
    outModel.Lights.clear(); // Assuming outModel has a std::vector<ModelLight> Lights;

    // Check if the model uses the light extension by seeing if the lights array exists
    if (!gltfModel.lights.empty())
    {
        for (const auto& gltfLight : gltfModel.lights)
        {
            ModelLight light{};
            light.name = gltfLight.name;

            // Set Color
            if (gltfLight.color.size() == 3) {
                light.color = XMFLOAT3((float)gltfLight.color[0], (float)gltfLight.color[1], (float)gltfLight.color[2]);
            }

            // Set Intensity and Range
            light.intensity = (float)gltfLight.intensity;
            light.range = (float)gltfLight.range;

            // Set Type
            if (gltfLight.type == "directional") {
                light.type = LightType::Directional;
            }
            else if (gltfLight.type == "point") {
                light.type = LightType::Point;
            }
            else if (gltfLight.type == "spot") {
                light.type = LightType::Spot;
                // Load spot-specific properties
                light.innerConeAngle = (float)gltfLight.spot.innerConeAngle;
                light.outerConeAngle = (float)gltfLight.spot.outerConeAngle;
            }

            outModel.Lights.push_back(light);
        }
    }

    // -----------------------------
    // Load Meshes
    // -----------------------------
    for (const auto& gltfMesh : gltfModel.meshes)
    {
        ModelMesh mesh{};
        mesh.Name = std::wstring(gltfMesh.name.begin(), gltfMesh.name.end());

        std::vector<ModelPrimitive> primitives;

        UINT currentBaseVertex = 0;
        UINT currentIndexLocation = 0;

        for (const auto& prim : gltfMesh.primitives)
        {
            // ---- Position ----
            const float* posData = nullptr;
            auto posIt = prim.attributes.find("POSITION");
            if (posIt == prim.attributes.end()) continue;
            const tinygltf::Accessor& posAcc = gltfModel.accessors[posIt->second];
            const tinygltf::BufferView& posView = gltfModel.bufferViews[posAcc.bufferView];
            const tinygltf::Buffer& posBuf = gltfModel.buffers[posView.buffer];
            posData = reinterpret_cast<const float*>(
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

            // ---- Tangent ---- 
            const float* tangentData = nullptr;
            auto tanIt = prim.attributes.find("TANGENT");
            if (tanIt != prim.attributes.end())
            {
                const tinygltf::Accessor& tanAcc = gltfModel.accessors[tanIt->second];
                const tinygltf::BufferView& tanView = gltfModel.bufferViews[tanAcc.bufferView];
                const tinygltf::Buffer& tanBuf = gltfModel.buffers[tanView.buffer];
                tangentData = reinterpret_cast<const float*>(
                    tanBuf.data.data() + tanView.byteOffset + tanAcc.byteOffset);
            }


            size_t vertCount = posAcc.count;
            for (size_t v = 0; v < vertCount; ++v)
            {
                ModelVertex mv{};
                mv.Position = XMFLOAT3(posData[v * 3 + 0], posData[v * 3 + 1], posData[v * 3 + 2]);

                if (normalData)
                {
                    mv.Normal = XMFLOAT3(normalData[v * 3 + 0], normalData[v * 3 + 1], normalData[v * 3 + 2]);
                }
                else
                {
                    mv.Normal = XMFLOAT3(0, 1, 0);
                }

                if (texData)
                {
                    mv.TexCoord = XMFLOAT2(texData[v * 2 + 0], texData[v * 2 + 1]);
                }
                else
                {
                    mv.TexCoord = XMFLOAT2(0, 0);
                }

                if (tangentData)
                {
                    mv.Tangent = XMFLOAT4(tangentData[v * 4 + 0], tangentData[v * 4 + 1], tangentData[v * 4 + 2], tangentData[v * 4 + 3]);
                }
                else
                {
                    mv.Tangent = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
                }

                mesh.Vertices.push_back(mv);
            }

            // ---- Indices ----
            size_t idxCount = 0;

            if (prim.indices > -1)
            {
                const tinygltf::Accessor& idxAcc = gltfModel.accessors[prim.indices];
                const tinygltf::BufferView& idxView = gltfModel.bufferViews[idxAcc.bufferView];
                const tinygltf::Buffer& idxBuf = gltfModel.buffers[idxView.buffer];
                const uint8_t* idxData = idxBuf.data.data() + idxView.byteOffset + idxAcc.byteOffset;

                idxCount = idxAcc.count;
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
                    }
                    mesh.Indices.push_back(idx);
                }
            }
            else
            {
                idxCount = vertCount;
                for (uint32_t i = 0; i < idxCount; ++i)
                {
                    mesh.Indices.push_back(i);
                }
            }

            ModelPrimitive primInfo{};
            primInfo.IndexCount = (UINT)idxCount;
            primInfo.StartIndexLocation = currentIndexLocation;
            primInfo.BaseVertexLocation = currentBaseVertex;
            primInfo.MaterialIndex = (prim.material < 0) ? 0 : prim.material;
            primitives.push_back(primInfo);

            currentBaseVertex += (UINT)vertCount;
            currentIndexLocation += (UINT)idxCount;
        }

        // --- Create the single, combined GPU buffers for the entire mesh ---
        if (!mesh.Vertices.empty())
        {
            UINT vbByteSize = (UINT)mesh.Vertices.size() * sizeof(ModelVertex);
            createGeometryVertexResource(device, cmdList, mesh, mesh.Vertices.data(), vbByteSize);
        }

        if (!mesh.Indices.empty())
        {
            UINT ibByteSize = (UINT)mesh.Indices.size() * sizeof(uint32_t);
            createGeometryIndexResource(device, cmdList, mesh, mesh.Indices.data(), ibByteSize, DXGI_FORMAT_R32_UINT);
        }

        mesh.Primitives = primitives;
        outModel.Meshes.push_back(std::move(mesh));
    }

    fprintf(gpFile, "\n--- glTF Model Summary ---\n");

    // Geometry & Mesh info
    fprintf(gpFile, "Number of Meshes: %zu\n", gltfModel.meshes.size());
    size_t totalPrimitives = 0;
    size_t totalVertices = 0;
    size_t totalIndices = 0;
    for (const auto& mesh : gltfModel.meshes)
    {
        for (const auto& prim : mesh.primitives)
        {
            const tinygltf::Accessor& posAcc = gltfModel.accessors[prim.attributes.at("POSITION")];
            totalVertices += posAcc.count;

            if (prim.indices > -1)
            {
                const tinygltf::Accessor& idxAcc = gltfModel.accessors[prim.indices];
                totalIndices += idxAcc.count;
            }
            else totalIndices += posAcc.count;

            totalPrimitives++;
        }
    }
    fprintf(gpFile, "Total Primitives: %zu\n", totalPrimitives);
    fprintf(gpFile, "Total Vertices:   %zu\n", totalVertices);
    fprintf(gpFile, "Total Indices:    %zu\n", totalIndices);

    // Materials & Textures
    fprintf(gpFile, "Number of Materials: %zu\n", gltfModel.materials.size());
    fprintf(gpFile, "Number of Textures:  %zu\n", gltfModel.textures.size());
    fprintf(gpFile, "Number of Images:    %zu\n", gltfModel.images.size());

    // Lights (already printed separately)
    fprintf(gpFile, "Number of Lights:    %zu\n", gltfModel.lights.size());

    // Cameras
    fprintf(gpFile, "Number of Cameras:   %zu\n", gltfModel.cameras.size());
    if (!gltfModel.cameras.empty())
    {
        for (size_t i = 0; i < gltfModel.cameras.size(); ++i)
        {
            const auto& cam = gltfModel.cameras[i];
            fprintf(gpFile, "  Camera[%zu]: type=%s\n", i, cam.type.c_str());
            if (cam.type == "perspective")
            {
                fprintf(gpFile, "    yfov=%.3f aspect=%.3f znear=%.3f zfar=%.3f\n",
                    (float)cam.perspective.yfov, (float)cam.perspective.aspectRatio,
                    (float)cam.perspective.znear, (float)cam.perspective.zfar);
            }
            else if (cam.type == "orthographic")
            {
                fprintf(gpFile, "    xmag=%.3f ymag=%.3f znear=%.3f zfar=%.3f\n",
                    (float)cam.orthographic.xmag, (float)cam.orthographic.ymag,
                    (float)cam.orthographic.znear, (float)cam.orthographic.zfar);
            }
        }
    }

    // Nodes / Instances
    fprintf(gpFile, "Number of Nodes:     %zu\n", gltfModel.nodes.size());
    size_t meshInstanceCount = 0;
    for (const auto& node : gltfModel.nodes)
    {
        if (node.mesh >= 0) meshInstanceCount++;
    }
    fprintf(gpFile, "Mesh Instances Found: %zu\n", meshInstanceCount);

    // Animations
    fprintf(gpFile, "Number of Animations: %zu\n", gltfModel.animations.size());
    if (!gltfModel.animations.empty())
    {
        for (size_t i = 0; i < gltfModel.animations.size(); ++i)
        {
            const auto& anim = gltfModel.animations[i];
            fprintf(gpFile, "  Animation[%zu]: channels=%zu samplers=%zu name=%s\n",
                i, anim.channels.size(), anim.samplers.size(), anim.name.c_str());
        }
    }

    // Environment
    bool hasEnvironment = gltfModel.extensions.find("KHR_lights_environment") != gltfModel.extensions.end();
    fprintf(gpFile, "Environment present: %s\n", hasEnvironment ? "YES" : "NO");

    fprintf(gpFile, "--- End of glTF Model Summary ---\n\n");

    return S_OK;
}