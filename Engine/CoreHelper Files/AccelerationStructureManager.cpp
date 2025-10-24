#include "AccelerationStructureManager.h"
#include "../RenderEngine Files/RenderEngine.h"
#include "CommonFunction.h"
#include <stack>
#include <algorithm>

// Static instance for the singleton
static std::unique_ptr<AccelerationStructureManager> s_instance;

AccelerationStructureManager* AccelerationStructureManager::Get()
{
    if (!s_instance) {
        s_instance = std::unique_ptr<AccelerationStructureManager>(new AccelerationStructureManager());
    }
    return s_instance.get();
}

HRESULT AccelerationStructureManager::Initialize(ID3D12Device* device, RenderEngine* engine)
{
    m_device = device;
    m_pRenderEngine = engine;

    m_uberTriangleBuffer.Stride = sizeof(Triangle);
    m_uberBlasNodeBuffer.Stride = sizeof(BVHNode);
    m_tlasNodeBuffer.Stride = sizeof(BVHNode);
    m_instanceDataBuffer.Stride = sizeof(ModelInstanceGPUData);

    return S_OK;
}

const BuiltBLAS* AccelerationStructureManager::GetOrBuildBLAS(ID3D12GraphicsCommandList* cmdList, Model* model)
{
    auto it = m_blasCache.find(model);
    if (it != m_blasCache.end())
    {
        return it->second.get();
    }

    return BuildBLASFromModel(cmdList, model);
}

const BuiltBLAS* AccelerationStructureManager::GetCachedBLAS(const Model* model) const
{
    auto it = m_blasCache.find(model);
    if (it != m_blasCache.end())
    {
        return it->second.get();
    }
    
    return nullptr;
}

BLASStats AccelerationStructureManager::AnalyzeBLAS(const BuiltBLAS* blas)
{
    BLASStats stats = {};
    if (!blas || blas->BaseNodeIndex >= m_allBlasNodes.size())
    {
        stats.minTrianglesPerLeaf = 0;
        return stats;
    }

    std::stack<std::pair<uint32_t, uint32_t>> stack; // Pair: {node global index, depth}
    stack.push({ blas->BaseNodeIndex, 1 });

    uint64_t totalTrianglesInLeaves = 0;

    while (!stack.empty())
    {
        auto [currentNodeIndex, currentDepth] = stack.top();
        stack.pop();

        stats.nodeCount++;
        stats.maxDepth = max(stats.maxDepth, currentDepth);

        const BVHNode& node = m_allBlasNodes[currentNodeIndex];

        if (node.triangleCount > 0) // It's a leaf node
        {
            stats.leafNodeCount++;
            stats.minTrianglesPerLeaf = min(stats.minTrianglesPerLeaf, (uint32_t)node.triangleCount);
            stats.maxTrianglesPerLeaf = max(stats.maxTrianglesPerLeaf, (uint32_t)node.triangleCount);
            totalTrianglesInLeaves += node.triangleCount;
        }
        else // It's an internal node
        {
            stats.internalNodeCount++;
            // Push children onto the stack to be processed.
            uint32_t leftChildIndex = node.leftChildOrFirstTriangleIndex;
            uint32_t rightChildIndex = leftChildIndex + 1;

            if (leftChildIndex < m_allBlasNodes.size()) stack.push({ leftChildIndex, currentDepth + 1 });
            if (rightChildIndex < m_allBlasNodes.size()) stack.push({ rightChildIndex, currentDepth + 1 });
        }
    }

    if (stats.leafNodeCount > 0)
    {
        stats.averageTrianglesPerLeaf = static_cast<float>(totalTrianglesInLeaves) / stats.leafNodeCount;
    }
    else
    {
        stats.minTrianglesPerLeaf = 0; // Handle case with no leaves
    }

    return stats;
}

const BuiltBLAS* AccelerationStructureManager::BuildBLASFromModel(ID3D12GraphicsCommandList* cmdList, Model* model)
{
    std::vector<Triangle> modelTriangles;

    // Go through each mesh in the model
    for (const auto& mesh : model->Meshes)
    {
        // Go through each primitive (sub-mesh with a material) in the mesh
        for (const auto& primitive : mesh.Primitives)
        {
            const auto& vertices = mesh.Vertices;
            const auto& indices = mesh.Indices;

            // Process the indices for this primitive only
            for (size_t i = 0; i < primitive.IndexCount; i += 3)
            {
                Triangle tri = {};

                // Get indices relative to this primitive's start location
                uint32_t i0 = indices[primitive.StartIndexLocation + i + 0];
                uint32_t i1 = indices[primitive.StartIndexLocation + i + 1];
                uint32_t i2 = indices[primitive.StartIndexLocation + i + 2];

                const ModelVertex& v0 = vertices[i0];
                const ModelVertex& v1 = vertices[i1];
                const ModelVertex& v2 = vertices[i2];

                tri.v0 = v0.Position;
                tri.v1 = v1.Position;
                tri.v2 = v2.Position;

                tri.n0 = v0.Normal;
                tri.n1 = v1.Normal;
                tri.n2 = v2.Normal;

                tri.tc0 = v0.TexCoord;
                tri.tc1 = v1.TexCoord;
                tri.tc2 = v2.TexCoord;

                if (primitive.MaterialIndex >= 0) {
                    tri.MaterialIndex = primitive.MaterialIndex;
                }
                else {
                    tri.MaterialIndex = 0; // Default material if none is specified
                }

                modelTriangles.push_back(tri);
            }
        }
    }

    // 2. Build the BLAS in local space
    std::vector<BVHNode> blasNodes;
    uint32_t baseTriangleIndex = static_cast<uint32_t>(m_allTriangles.size());
    BVHBuilder::Build(modelTriangles, blasNodes, baseTriangleIndex);

    // 3. Store the results and update the uber-buffers
    auto builtBlas = std::make_unique<BuiltBLAS>();
    builtBlas->BaseTriangleIndex = baseTriangleIndex;
    builtBlas->BaseNodeIndex = static_cast<uint32_t>(m_allBlasNodes.size());
    for (auto& node : blasNodes)
    {
        if (node.triangleCount == 0) 
        {
            node.leftChildOrFirstTriangleIndex += builtBlas->BaseNodeIndex;
        }
        else
        {
            node.leftChildOrFirstTriangleIndex += baseTriangleIndex;
        }
    }
    builtBlas->RootNode = blasNodes.empty() ? BVHNode{} : blasNodes[0];

    m_allTriangles.insert(m_allTriangles.end(), modelTriangles.begin(), modelTriangles.end());
    m_allBlasNodes.insert(m_allBlasNodes.end(), blasNodes.begin(), blasNodes.end());

    m_uberTriangleBuffer.Sync(m_pRenderEngine, cmdList, m_allTriangles.data(), m_allTriangles.size());
    if (m_uberTriangleBuffer.GpuResourceDirty) {
        m_staticGeometrySrvsDirty = true; 
        if (m_uberTriangleBuffer.Resource) m_uberTriangleBuffer.Resource->SetName(L"Uber Triangle Buffer");
    }

    m_uberBlasNodeBuffer.Sync(m_pRenderEngine, cmdList, m_allBlasNodes.data(), m_allBlasNodes.size());
    if (m_uberBlasNodeBuffer.GpuResourceDirty) {
        m_staticGeometrySrvsDirty = true; 
        if (m_uberBlasNodeBuffer.Resource) m_uberBlasNodeBuffer.Resource->SetName(L"Uber BLAS Node Buffer");
    }

    // 4. Return a handle
    const Model* modelKey = model;
    m_blasCache[modelKey] = std::move(builtBlas);
    return m_blasCache[modelKey].get();
}

void AccelerationStructureManager::BuildTLAS(const std::vector<ModelInstance>& instances)
{
    if (instances.empty())
    {
        m_tlasNodes.clear();
        return;
    }

    TLASBuilder::Build(instances, m_tlasNodes, this);
}

void AccelerationStructureManager::UpdateGpuBuffers(ID3D12GraphicsCommandList* cmdList, const std::vector<ModelInstance>& instances)
{
    std::vector<ModelInstanceGPUData> instanceGpuData;
    if (!instances.empty())
    {
        instanceGpuData.reserve(instances.size());
        for (const auto& inst : instances)
        {
            const BuiltBLAS* blas = GetCachedBLAS(inst.SourceModel);
            if (!blas) continue;

            ModelInstanceGPUData data = {};
            data.Transform = inst.Transform;
            data.InverseTransform = inst.InverseTransform;
            data.BaseTriangleIndex = blas->BaseTriangleIndex;
            data.BaseNodeIndex = blas->BaseNodeIndex;
            data.MaterialOffset = inst.MaterialOffset;
            instanceGpuData.push_back(data);
        }
    }

    m_tlasNodeBuffer.Sync(m_pRenderEngine, cmdList, m_tlasNodes.data(), m_tlasNodes.size());
    if (m_tlasNodeBuffer.GpuResourceDirty) {
        m_instanceSrvsDirty = true; 
        if (m_tlasNodeBuffer.Resource) m_tlasNodeBuffer.Resource->SetName(L"TLAS Node Buffer");
    }

    m_instanceDataBuffer.Sync(m_pRenderEngine, cmdList, instanceGpuData.data(), instanceGpuData.size());
    if (m_instanceDataBuffer.GpuResourceDirty) {
        m_instanceSrvsDirty = true; 
        if (m_instanceDataBuffer.Resource) m_instanceDataBuffer.Resource->SetName(L"Instance Data Buffer");
    }

}

void AccelerationStructureManager::RefitNodeRecursive(int nodeIndex, const std::vector<ModelInstance>& instances)
{
    // Get a reference to the current node in the TLAS.
    BVHNode& node = m_tlasNodes[nodeIndex];

    if (node.triangleCount > 0) // It's a leaf node.
    {
        // if leaaf node ,update AABB

        uint32_t instanceIndex = node.leftChildOrFirstTriangleIndex;
        const auto& inst = instances[instanceIndex];

        const BuiltBLAS* blas = GetCachedBLAS(inst.SourceModel);
        if (!blas) return;

        DirectX::XMFLOAT3 newWorldMin, newWorldMax;
        TLASBuilder::TransformAABB(blas->RootNode.aabbMin, blas->RootNode.aabbMax, inst.Transform, newWorldMin, newWorldMax);

        // Update the node's bounds.
        node.aabbMin = newWorldMin;
        node.aabbMax = newWorldMax;
    }
    else // It's an internal node.
    {
        RefitNodeRecursive(node.leftChildOrFirstTriangleIndex, instances);     // Recurse on the left child
        RefitNodeRecursive(node.leftChildOrFirstTriangleIndex + 1, instances); // Recurse on the right child

        // After the children have been updated, we can update this parent node.
        // The parent's AABB is the union of its two children's AABBs.
        const BVHNode& leftChild = m_tlasNodes[node.leftChildOrFirstTriangleIndex];
        const BVHNode& rightChild = m_tlasNodes[node.leftChildOrFirstTriangleIndex + 1];

        using namespace DirectX;
        XMVECTOR leftMinV = XMLoadFloat3(&leftChild.aabbMin);
        XMVECTOR leftMaxV = XMLoadFloat3(&leftChild.aabbMax);
        XMVECTOR rightMinV = XMLoadFloat3(&rightChild.aabbMin);
        XMVECTOR rightMaxV = XMLoadFloat3(&rightChild.aabbMax);

        XMStoreFloat3(&node.aabbMin, XMVectorMin(leftMinV, rightMinV));
        XMStoreFloat3(&node.aabbMax, XMVectorMax(leftMaxV, rightMaxV));
    }
}

void AccelerationStructureManager::RefitTLAS(const std::vector<ModelInstance>& instances)
{
    if (m_tlasNodes.empty() || instances.empty())
    {
        return;
    }

    RefitNodeRecursive(0, instances);
}