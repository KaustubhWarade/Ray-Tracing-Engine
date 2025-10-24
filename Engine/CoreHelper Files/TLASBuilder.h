#pragma once

#include "Model Loader\ModelLoader.h" 
#include "Mesh.h" 

struct TLASPrimitive
{
    DirectX::XMFLOAT3 aabbMin;
    DirectX::XMFLOAT3 aabbMax;
    uint32_t instanceIndex;
};

class AccelerationStructureManager;

namespace TLASBuilder
{
    void Build(
        const std::vector<ModelInstance>& instances,
        std::vector<BVHNode>& outTlasNodes,
        const AccelerationStructureManager* pAccelManager
    );
    void TransformAABB(const DirectX::XMFLOAT3& localMin, const DirectX::XMFLOAT3& localMax, const DirectX::XMMATRIX& transform, DirectX::XMFLOAT3& outWorldMin, DirectX::XMFLOAT3& outWorldMax);
}
