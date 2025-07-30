#include "BVHBuilder.h"
#include <algorithm>

// Helper to compute the AABB for a range of triangles
void ComputeAABB(const std::vector<Triangle>& triangles, uint32_t startIndex, uint32_t count, DirectX::XMFLOAT3& outMin, DirectX::XMFLOAT3& outMax)
{
    outMin = { FLT_MAX, FLT_MAX, FLT_MAX };
    outMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    for (uint32_t i = 0; i < count; ++i) {
        const Triangle& tri = triangles[startIndex + i];
        auto min_v = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return DirectX::XMFLOAT3(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)); };
        auto max_v = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return DirectX::XMFLOAT3(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)); };

        outMin = min_v(outMin, tri.v0); outMin = min_v(outMin, tri.v1); outMin = min_v(outMin, tri.v2);
        outMax = max_v(outMax, tri.v0); outMax = max_v(outMax, tri.v1); outMax = max_v(outMax, tri.v2);
    }
}

float SurfaceArea(const DirectX::XMFLOAT3& aabbMin, const DirectX::XMFLOAT3& aabbMax)
{
    // Calculate the lengths of the sides of the box
    DirectX::XMFLOAT3 extent = {
        aabbMax.x - aabbMin.x,
        aabbMax.y - aabbMin.y,
        aabbMax.z - aabbMin.z
    };

    // The surface area of a rectangular prism is 2 * (width*height + width*depth + height*depth)
    return 2.0f * (extent.x * extent.y + extent.x * extent.z + extent.y * extent.z);
}


// The main recursive builder function
void SubdivideNode(BVHNode& node, std::vector<Triangle>& triangles, std::vector<BVHNode>& bvhNodes, uint32_t startIndex, uint32_t count, uint32_t& nodesUsed)
{
    // If a node has 4 or fewer triangles, it becomes a leaf. This is a tunable parameter.
    if (count <= 4) {
        node.leftChildOrFirstTriangleIndex = startIndex;
        node.triangleCount = count;
        return;
    }

    float bestCost = FLT_MAX;
    int bestAxis = -1;
    uint32_t bestSplitIndex = 0;

    // Iterate through all three axes to find the best split
    for (int axis = 0; axis < 3; ++axis)
    {
        // Sort triangles along the current axis based on their centroid
        std::sort(triangles.begin() + startIndex, triangles.begin() + startIndex + count,
            [axis](const Triangle& a, const Triangle& b) {
                float centerA = (a.v0.x + a.v1.x + a.v2.x) * 0.3333f;
                if (axis == 1) centerA = (a.v0.y + a.v1.y + a.v2.y) * 0.3333f;
                if (axis == 2) centerA = (a.v0.z + a.v1.z + a.v2.z) * 0.3333f;

                float centerB = (b.v0.x + b.v1.x + b.v2.x) * 0.3333f;
                if (axis == 1) centerB = (b.v0.y + b.v1.y + b.v2.y) * 0.3333f;
                if (axis == 2) centerB = (b.v0.z + b.v1.z + b.v2.z) * 0.3333f;

                return centerA < centerB;
            });

        // Test N-1 possible splits between the sorted triangles
        for (uint32_t i = 1; i < count; ++i)
        {
            uint32_t splitIndex = startIndex + i;

            // Compute AABB for the left and right sides of the potential split
            DirectX::XMFLOAT3 leftMin, leftMax, rightMin, rightMax;
            ComputeAABB(triangles, startIndex, i, leftMin, leftMax); // Left side has 'i' triangles
            ComputeAABB(triangles, splitIndex, count - i, rightMin, rightMax); // Right side has 'count - i' triangles

            // Calculate the SAH cost for this split
            float cost = SurfaceArea(leftMin, leftMax) * i + SurfaceArea(rightMin, rightMax) * (count - i);

            if (cost < bestCost)
            {
                bestCost = cost;
                bestAxis = axis;
                bestSplitIndex = splitIndex;
            }
        }
    }

    // If we didn't find a good split (e.g., all triangles are at the same point), fall back to a simple split
    if (bestAxis == -1)
    {
        bestSplitIndex = startIndex + count / 2;
    }
    else
    {
        // Re-sort the triangles along the best axis found to ensure the partition is correct
        std::sort(triangles.begin() + startIndex, triangles.begin() + startIndex + count,
            [bestAxis](const Triangle& a, const Triangle& b) {
                float centerA = (a.v0.x + a.v1.x + a.v2.x) * 0.3333f;
                if (bestAxis == 1) centerA = (a.v0.y + a.v1.y + a.v2.y) * 0.3333f;
                if (bestAxis == 2) centerA = (a.v0.z + a.v1.z + a.v2.z) * 0.3333f;

                float centerB = (b.v0.x + b.v1.x + b.v2.x) * 0.3333f;
                if (bestAxis == 1) centerB = (b.v0.y + b.v1.y + b.v2.y) * 0.3333f;
                if (bestAxis == 2) centerB = (b.v0.z + b.v1.z + b.v2.z) * 0.3333f;

                return centerA < centerB;
            });
    }

    uint32_t leftCount = bestSplitIndex - startIndex;
    if (leftCount == 0 || leftCount == count) {
        leftCount = count / 2;
    }

    // If the partition failed (all triangles on one side), force a 50/50 split.
    if (leftCount == 0 || leftCount == count) {
        leftCount = count / 2;
    }

    // 3. Create child nodes
    uint32_t leftChildIndex = nodesUsed++;
    uint32_t rightChildIndex = nodesUsed++;

    // The current node is now an internal node
    node.leftChildOrFirstTriangleIndex = leftChildIndex;
    node.triangleCount = 0;

    // 4. Initialize children: compute their AABBs and recurse
    BVHNode& leftChild = bvhNodes[leftChildIndex];
    uint32_t leftStartIndex = startIndex;
    ComputeAABB(triangles, leftStartIndex, leftCount, leftChild.aabbMin, leftChild.aabbMax);

    BVHNode& rightChild = bvhNodes[rightChildIndex];
    uint32_t rightStartIndex = startIndex + leftCount;
    int rightCount = count - leftCount;
    ComputeAABB(triangles, rightStartIndex, rightCount, rightChild.aabbMin, rightChild.aabbMax);

    SubdivideNode(leftChild, triangles, bvhNodes, leftStartIndex, leftCount, nodesUsed);
    SubdivideNode(rightChild, triangles, bvhNodes, rightStartIndex, rightCount, nodesUsed);
}

void BVHBuilder::Build(std::vector<Triangle>& triangles, std::vector<BVHNode>& outBvhNodes)
{
    outBvhNodes.resize(triangles.size() * 2);

    BVHNode& root = outBvhNodes[0];
    ComputeAABB(triangles, 0, (uint32_t)triangles.size(), root.aabbMin, root.aabbMax);

    uint32_t nodesUsed = 1; // 0 is root

    // Start the recursive subdivision
    SubdivideNode(root, triangles, outBvhNodes, 0, (uint32_t)triangles.size(), nodesUsed);

    outBvhNodes.resize(nodesUsed);
}

