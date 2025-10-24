
#include "TLASBuilder.h"
#include "AccelerationStructureManager.h"
#include <algorithm>

namespace TLASBuilder
{
    void TransformAABB(const DirectX::XMFLOAT3& localMin, const DirectX::XMFLOAT3& localMax, const DirectX::XMMATRIX& transform, DirectX::XMFLOAT3& outWorldMin, DirectX::XMFLOAT3& outWorldMax)
    {
        using namespace DirectX;

        // Get the 8 corners of the local AABB
        XMVECTOR corners[8] = {
            XMVectorSet(localMin.x, localMin.y, localMin.z, 1.0f),
            XMVectorSet(localMax.x, localMin.y, localMin.z, 1.0f),
            XMVectorSet(localMin.x, localMax.y, localMin.z, 1.0f),
            XMVectorSet(localMax.x, localMax.y, localMin.z, 1.0f),
            XMVectorSet(localMin.x, localMin.y, localMax.z, 1.0f),
            XMVectorSet(localMax.x, localMin.y, localMax.z, 1.0f),
            XMVectorSet(localMin.x, localMax.y, localMax.z, 1.0f),
            XMVectorSet(localMax.x, localMax.y, localMax.z, 1.0f),
        };

        // Transform corners to world space
        for (int i = 0; i < 8; ++i)
        {
            corners[i] = XMVector3Transform(corners[i], transform);
        }

        XMVECTOR worldMin = corners[0];
        XMVECTOR worldMax = corners[0];
        for (int i = 1; i < 8; ++i)
        {
            worldMin = XMVectorMin(worldMin, corners[i]);
            worldMax = XMVectorMax(worldMax, corners[i]);
        }
        XMStoreFloat3(&outWorldMin, worldMin);
        XMStoreFloat3(&outWorldMax, worldMax);
    }

    void ComputeBounds(const std::vector<TLASPrimitive>& primitives, uint32_t startIndex, uint32_t count, DirectX::XMFLOAT3& outMin, DirectX::XMFLOAT3& outMax)
    {
        outMin = { FLT_MAX, FLT_MAX, FLT_MAX };
        outMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        auto min_v = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return DirectX::XMFLOAT3(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)); };
        auto max_v = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return DirectX::XMFLOAT3(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)); };

        for (uint32_t i = 0; i < count; ++i)
        {
            const auto& prim = primitives[startIndex + i];
            outMin = min_v(outMin, prim.aabbMin);
            outMax = max_v(outMax, prim.aabbMax);
        }
    }

    // The main recursive builder function for the TLAS
    void Subdivide(BVHNode& node, std::vector<TLASPrimitive>& primitives, std::vector<BVHNode>& tlasNodes, uint32_t startIndex, uint32_t count, uint32_t& nodesUsed)
    {
        if (count <= 1) {
            node.leftChildOrFirstTriangleIndex = primitives[startIndex].instanceIndex;
            node.triangleCount = 1;
            return;
        }

        DirectX::XMFLOAT3 extent = { node.aabbMax.x - node.aabbMin.x, node.aabbMax.y - node.aabbMin.y, node.aabbMax.z - node.aabbMin.z };
        int axis = 0;
        if (extent.y > extent.x) axis = 1;
        if (extent.z > extent.y && extent.z > extent.x) axis = 2;

        float splitPos = node.aabbMin.x + extent.x * 0.5f;
        if (axis == 1) splitPos = node.aabbMin.y + extent.y * 0.5f;
        if (axis == 2) splitPos = node.aabbMin.z + extent.z * 0.5f;

        auto partition_iterator = std::partition(primitives.begin() + startIndex, primitives.begin() + startIndex + count,
            [axis, splitPos](const TLASPrimitive& prim) {
                float center = (prim.aabbMin.x + prim.aabbMax.x) * 0.5f;
                if (axis == 1) center = (prim.aabbMin.y + prim.aabbMax.y) * 0.5f;
                if (axis == 2) center = (prim.aabbMin.z + prim.aabbMax.z) * 0.5f;
                return center < splitPos;
            });

        int leftCount = static_cast<int>(std::distance(primitives.begin() + startIndex, partition_iterator));
        if (leftCount == 0 || leftCount == count) {
            leftCount = count / 2;
        }

        uint32_t leftChildIndex = nodesUsed++;
        uint32_t rightChildIndex = nodesUsed++;

        node.leftChildOrFirstTriangleIndex = leftChildIndex;
        node.triangleCount = 0; // Internal node

        BVHNode& leftChild = tlasNodes[leftChildIndex];
        ComputeBounds(primitives, startIndex, leftCount, leftChild.aabbMin, leftChild.aabbMax);

        BVHNode& rightChild = tlasNodes[rightChildIndex];
        ComputeBounds(primitives, startIndex + leftCount, count - leftCount, rightChild.aabbMin, rightChild.aabbMax);

        Subdivide(leftChild, primitives, tlasNodes, startIndex, leftCount, nodesUsed);
        Subdivide(rightChild, primitives, tlasNodes, startIndex + leftCount, count - leftCount, nodesUsed);
    }

    void Build(
        const std::vector<ModelInstance>& instances,
        std::vector<BVHNode>& outTlasNodes,
        const AccelerationStructureManager* pAccelManager)
    {
        if (instances.empty()) {
            outTlasNodes.clear();
            return;
        }

        // 1. Create primitives for the builder (world-space AABBs)
        std::vector<TLASPrimitive> primitives;
        primitives.reserve(instances.size());
        for (uint32_t i = 0; i < instances.size(); ++i)
        {
            const auto& inst = instances[i];

            auto nonConstManager = const_cast<AccelerationStructureManager*>(pAccelManager);
            const BuiltBLAS* builtBlas = nonConstManager->GetCachedBLAS(inst.SourceModel);
            if (!builtBlas) continue;

            const BVHNode& rootBlasNode = builtBlas->RootNode;

            TLASPrimitive prim;
            prim.instanceIndex = i;

            TransformAABB(rootBlasNode.aabbMin, rootBlasNode.aabbMax, inst.Transform, prim.aabbMin, prim.aabbMax);
            primitives.push_back(prim);
        }

        // 2. Build the BVH
        outTlasNodes.resize(primitives.size() * 2);

        BVHNode& root = outTlasNodes[0];
        ComputeBounds(primitives, 0, (uint32_t)primitives.size(), root.aabbMin, root.aabbMax);

        uint32_t nodesUsed = 1;
        Subdivide(root, primitives, outTlasNodes, 0, (uint32_t)primitives.size(), nodesUsed);

        outTlasNodes.resize(nodesUsed);
    }
}