#pragma once

#include "RenderEngine Files/global.h"
#include "Model Loader\ModelLoader.h"
#include "Mesh.h"
#include "TLASBuilder.h"
#include "BVHBuilder.h"
#include "GpuBuffer.h"

// A handle to refer to a built BLAS, hiding the implementation details.
using BLASHandle = size_t;

struct BuiltBLAS
{
    BVHNode RootNode; 

    uint32_t BaseTriangleIndex;
    uint32_t BaseNodeIndex;
};

struct BLASStats
{
    uint32_t nodeCount = 0;
    uint32_t leafNodeCount = 0;
    uint32_t internalNodeCount = 0;
    uint32_t maxDepth = 0;
    uint32_t minTrianglesPerLeaf = (std::numeric_limits<uint32_t>::max)();
    uint32_t maxTrianglesPerLeaf = 0;
    float averageTrianglesPerLeaf = 0.0f;
};

class RenderEngine;

class AccelerationStructureManager
{
public:
    AccelerationStructureManager() = default;

    static AccelerationStructureManager* Get();

    HRESULT Initialize(ID3D12Device* device, RenderEngine* engine);

    const BuiltBLAS* GetOrBuildBLAS(ID3D12GraphicsCommandList* cmdList, Model* model);

    const BuiltBLAS* GetCachedBLAS(const Model* model) const;

    BLASStats AnalyzeBLAS(const BuiltBLAS* blas);

    void BuildTLAS(const std::vector<ModelInstance>& instances);
    void UpdateGpuBuffers(ID3D12GraphicsCommandList* cmdList, const std::vector<ModelInstance>& instances);
    void RefitTLAS(const std::vector<ModelInstance>& instances);

    bool StaticGeometrySrvsNeedUpdate() { bool dirty = m_staticGeometrySrvsDirty; m_staticGeometrySrvsDirty = false; return dirty; }
    bool InstanceSrvsNeedUpdate() { bool dirty = m_instanceSrvsDirty; m_instanceSrvsDirty = false; return dirty; }

    ID3D12Resource* GetUberTriangleBuffer() const { return m_uberTriangleBuffer.Resource.Get(); }
    UINT GetUberTriangleCount() const { return m_uberTriangleBuffer.Size; }
    UINT GetUberTriangleBufferStride() const { return m_uberTriangleBuffer.Stride; }

    ID3D12Resource* GetUberBlasNodeBuffer() const { return m_uberBlasNodeBuffer.Resource.Get(); }
    UINT GetUberBlasNodeCount() const { return m_uberBlasNodeBuffer.Size; }
    UINT GetUberBlasNodeBufferStride() const { return m_uberBlasNodeBuffer.Stride; }

    ID3D12Resource* GetTLASBuffer() const { return m_tlasNodeBuffer.Resource.Get(); }
    UINT GetTLASNodeCount() const { return m_tlasNodeBuffer.Size; }
    UINT GetTLASBufferStride() const { return m_tlasNodeBuffer.Stride; }

    ID3D12Resource* GetInstanceBuffer() const { return m_instanceDataBuffer.Resource.Get(); }
    UINT GetInstanceCount() const { return m_instanceDataBuffer.Size; }
    UINT GetInstanceBufferStride() const { return m_instanceDataBuffer.Stride; }



private:
    RenderEngine* m_pRenderEngine = nullptr;

    void RefitNodeRecursive(int nodeIndex, const std::vector<ModelInstance>& instances);

    const BuiltBLAS* BuildBLASFromModel(ID3D12GraphicsCommandList* cmdList, Model* model);

    ID3D12Device* m_device = nullptr;

    bool m_staticGeometrySrvsDirty = false;
    bool m_instanceSrvsDirty = false;

    ResizableBuffer m_uberTriangleBuffer;
    ResizableBuffer m_uberBlasNodeBuffer;
    ResizableBuffer m_tlasNodeBuffer;
    ResizableBuffer m_instanceDataBuffer;

    // CPU-side data
    std::vector<Triangle> m_allTriangles;
    std::vector<BVHNode> m_allBlasNodes;
    std::vector<BVHNode> m_tlasNodes;

    std::unordered_map<const Model*, std::unique_ptr<BuiltBLAS>> m_blasCache;
};