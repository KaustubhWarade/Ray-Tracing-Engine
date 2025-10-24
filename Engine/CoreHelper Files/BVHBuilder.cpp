#include "BVHBuilder.h"
#include <algorithm>
#include <atomic> 
#include <future>

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
void SubdivideSortNode(BVHNode& node, std::vector<Triangle>& triangles, std::vector<BVHNode>& bvhNodes, uint32_t startIndex, uint32_t count, uint32_t baseTriangleIndex, uint32_t& nodesUsed)
{
    // If a node has 4 or fewer triangles, it becomes a leaf. This is a tunable parameter.
    if (count <= 4 ) {
        node.leftChildOrFirstTriangleIndex = baseTriangleIndex + startIndex;
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

    SubdivideSortNode(leftChild, triangles, bvhNodes, leftStartIndex, leftCount, baseTriangleIndex,nodesUsed);
    SubdivideSortNode(rightChild, triangles, bvhNodes, rightStartIndex, rightCount, baseTriangleIndex,nodesUsed);
}

void SubdivideBinningPartitionNode(BVHNode& node, std::vector<Triangle>& triangles, std::vector<BVHNode>& bvhNodes, uint32_t startIndex, uint32_t count, uint32_t baseTriangleIndex, uint32_t& nodesUsed)
{
    // Leaf node condition: Stop if the number of triangles is small.
    // This is our primary termination criteria.
    if (count <= 4) {
        node.leftChildOrFirstTriangleIndex = baseTriangleIndex + startIndex;
        node.triangleCount = count;
        return;
    }

    // --- Binned SAH Implementation ---
    const int NUM_BINS = 16;
    float bestCost = FLT_MAX;
    int bestAxis = -1;
    int bestSplitBinIndex = 0; // The index of the first bin on the right side of the split

    // 1. Iterate through each axis to find the best possible split.
    for (int axis = 0; axis < 3; ++axis)
    {
        Bin bins[NUM_BINS];
        DirectX::XMFLOAT3 extent = { node.aabbMax.x - node.aabbMin.x, node.aabbMax.y - node.aabbMin.y, node.aabbMax.z - node.aabbMin.z };

        float axisMin, axisExtent;
        if (axis == 0) { axisMin = node.aabbMin.x; axisExtent = extent.x; }
        else if (axis == 1) { axisMin = node.aabbMin.y; axisExtent = extent.y; }
        else { axisMin = node.aabbMin.z; axisExtent = extent.z; }

        // If the node is flat along this axis, we can't split it.
        if (axisExtent < 1e-6f) continue;

        // 2. Populate the bins for the current axis in a single pass over the triangles.
        for (uint32_t i = 0; i < count; ++i)
        {
            Triangle& tri = triangles[startIndex + i];
            float centroid;
            if (axis == 0) centroid = (tri.v0.x + tri.v1.x + tri.v2.x) * 0.3333f;
            else if (axis == 1) centroid = (tri.v0.y + tri.v1.y + tri.v2.y) * 0.3333f;
            else centroid = (tri.v0.z + tri.v1.z + tri.v2.z) * 0.3333f;

            int binIndex = static_cast<int>(NUM_BINS * ((centroid - axisMin) / axisExtent));
            binIndex = std::clamp(binIndex, 0, NUM_BINS - 1);

            bins[binIndex].triangleCount++;
            auto min_v = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return DirectX::XMFLOAT3(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)); };
            auto max_v = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return DirectX::XMFLOAT3(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)); };

            bins[binIndex].aabbMin = min_v(bins[binIndex].aabbMin, tri.v0);
            bins[binIndex].aabbMin = min_v(bins[binIndex].aabbMin, tri.v1);
            bins[binIndex].aabbMin = min_v(bins[binIndex].aabbMin, tri.v2);
            bins[binIndex].aabbMax = max_v(bins[binIndex].aabbMax, tri.v0);
            bins[binIndex].aabbMax = max_v(bins[binIndex].aabbMax, tri.v1);
            bins[binIndex].aabbMax = max_v(bins[binIndex].aabbMax, tri.v2);
        }

        // 3. Evaluate the N-1 possible split planes between the bins.
        float leftArea[NUM_BINS - 1], rightArea[NUM_BINS - 1];
        int leftCount[NUM_BINS - 1], rightCount[NUM_BINS - 1];

        DirectX::XMFLOAT3 leftBoxMin = { FLT_MAX, FLT_MAX, FLT_MAX }, leftBoxMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        int leftSum = 0;
        for (int i = 0; i < NUM_BINS - 1; ++i)
        {
            leftSum += bins[i].triangleCount;
            leftCount[i] = leftSum;
            auto min_v = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return DirectX::XMFLOAT3(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)); };
            auto max_v = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return DirectX::XMFLOAT3(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)); };

            leftBoxMin = min_v(leftBoxMin, bins[i].aabbMin);
            leftBoxMax = max_v(leftBoxMax, bins[i].aabbMax);
            leftArea[i] = SurfaceArea(leftBoxMin, leftBoxMax);
        }

        DirectX::XMFLOAT3 rightBoxMin = { FLT_MAX, FLT_MAX, FLT_MAX }, rightBoxMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        int rightSum = 0;
        for (int i = NUM_BINS - 1; i > 0; --i)
        {
            rightSum += bins[i].triangleCount;
            rightCount[i - 1] = rightSum;
            auto min_v = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return DirectX::XMFLOAT3(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)); };
            auto max_v = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return DirectX::XMFLOAT3(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)); };
            rightBoxMin = min_v(rightBoxMin, bins[i].aabbMin);
            rightBoxMax = max_v(rightBoxMax, bins[i].aabbMax);
            rightArea[i - 1] = SurfaceArea(rightBoxMin, rightBoxMax);
        }

        // Calculate the cost for each potential split and find the best one.
        for (int i = 0; i < NUM_BINS - 1; ++i)
        {
            if (leftCount[i] > 0 && rightCount[i] > 0)
            {
                float cost = leftArea[i] * leftCount[i] + rightArea[i] * rightCount[i];
                if (cost < bestCost)
                {
                    bestCost = cost;
                    bestAxis = axis;
                    bestSplitBinIndex = i + 1;
                }
            }
        }
    }

    // If we didn't find a good split (e.g., cost is higher than not splitting), make this a leaf.
    float parentCost = SurfaceArea(node.aabbMin, node.aabbMax) * count;
    if (bestAxis == -1 || bestCost >= parentCost)
    {
        node.leftChildOrFirstTriangleIndex = baseTriangleIndex + startIndex;
        node.triangleCount = count;
        return;
    }

    // 4. Partition the triangles in-place using our manual swapping method.
    float splitPos;
    {
        DirectX::XMFLOAT3 extent = { node.aabbMax.x - node.aabbMin.x, node.aabbMax.y - node.aabbMin.y, node.aabbMax.z - node.aabbMin.z };
        float axisMin, axisExtent;
        if (bestAxis == 0) { axisMin = node.aabbMin.x; axisExtent = extent.x; }
        else if (bestAxis == 1) { axisMin = node.aabbMin.y; axisExtent = extent.y; }
        else { axisMin = node.aabbMin.z; axisExtent = extent.z; }
        splitPos = axisMin + axisExtent * (bestSplitBinIndex / (float)NUM_BINS);
    }

    uint32_t j = startIndex;
    for (uint32_t i = startIndex; i < startIndex + count; ++i)
    {
        float center;
        if (bestAxis == 0) center = (triangles[i].v0.x + triangles[i].v1.x + triangles[i].v2.x) * 0.3333f;
        else if (bestAxis == 1) center = (triangles[i].v0.y + triangles[i].v1.y + triangles[i].v2.y) * 0.3333f;
        else center = (triangles[i].v0.z + triangles[i].v1.z + triangles[i].v2.z) * 0.3333f;

        if (center < splitPos)
        {
            std::swap(triangles[i], triangles[j]);
            j++;
        }
    }
    int leftCount = j - startIndex;

    // Robustness: If the partition failed, force a 50/50 split to ensure progress.
    if (leftCount == 0 || leftCount == count) {
        leftCount = count / 2;
    }

    // 5. Create child nodes and recurse.
    uint32_t leftChildIndex = nodesUsed++;
    uint32_t rightChildIndex = nodesUsed++;

    node.leftChildOrFirstTriangleIndex = leftChildIndex;
    node.triangleCount = 0; // Mark as internal node

    BVHNode& leftChild = bvhNodes[leftChildIndex];
    ComputeAABB(triangles, startIndex, leftCount, leftChild.aabbMin, leftChild.aabbMax);

    BVHNode& rightChild = bvhNodes[rightChildIndex];
    ComputeAABB(triangles, startIndex + leftCount, count - leftCount, rightChild.aabbMin, rightChild.aabbMax);

    SubdivideBinningPartitionNode(leftChild, triangles, bvhNodes, startIndex, leftCount, baseTriangleIndex, nodesUsed);
    SubdivideBinningPartitionNode(rightChild, triangles, bvhNodes, startIndex + leftCount, count - leftCount, baseTriangleIndex, nodesUsed);
}

void SubdivideNode_Parallel(BVHNode& node, std::vector<Triangle>& triangles, std::vector<BVHNode>& bvhNodes, uint32_t startIndex, uint32_t count, uint32_t baseTriangleIndex, std::atomic<uint32_t>& atomic_nodesUsed, int depth)
{
    if (count <= 4) {
        node.leftChildOrFirstTriangleIndex = baseTriangleIndex + startIndex;
        node.triangleCount = count;
        return;
    }

    const int NUM_BINS = 16;
    float bestCost = FLT_MAX;
    int bestAxis = -1;
    int bestSplitBinIndex = 0;

    for (int axis = 0; axis < 3; ++axis)
    {
        Bin bins[NUM_BINS];
        DirectX::XMFLOAT3 extent = { node.aabbMax.x - node.aabbMin.x, node.aabbMax.y - node.aabbMin.y, node.aabbMax.z - node.aabbMin.z };
        float axisMin, axisExtent;
        if (axis == 0) { axisMin = node.aabbMin.x; axisExtent = extent.x; }
        else if (axis == 1) { axisMin = node.aabbMin.y; axisExtent = extent.y; }
        else { axisMin = node.aabbMin.z; axisExtent = extent.z; }
        if (axisExtent < 1e-6f) continue;

        for (uint32_t i = 0; i < count; ++i)
        {
            Triangle& tri = triangles[startIndex + i];
            float centroid;
            if (axis == 0) centroid = (tri.v0.x + tri.v1.x + tri.v2.x) * 0.3333f;
            else if (axis == 1) centroid = (tri.v0.y + tri.v1.y + tri.v2.y) * 0.3333f;
            else centroid = (tri.v0.z + tri.v1.z + tri.v2.z) * 0.3333f;
            int binIndex = static_cast<int>(NUM_BINS * ((centroid - axisMin) / axisExtent));
            binIndex = std::clamp(binIndex, 0, NUM_BINS - 1);
            bins[binIndex].triangleCount++;
            auto min_v = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return DirectX::XMFLOAT3(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)); };
            auto max_v = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return DirectX::XMFLOAT3(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)); };
            bins[binIndex].aabbMin = min_v(bins[binIndex].aabbMin, tri.v0); bins[binIndex].aabbMin = min_v(bins[binIndex].aabbMin, tri.v1); bins[binIndex].aabbMin = min_v(bins[binIndex].aabbMin, tri.v2);
            bins[binIndex].aabbMax = max_v(bins[binIndex].aabbMax, tri.v0); bins[binIndex].aabbMax = max_v(bins[binIndex].aabbMax, tri.v1); bins[binIndex].aabbMax = max_v(bins[binIndex].aabbMax, tri.v2);
        }
        float leftArea[NUM_BINS - 1], rightArea[NUM_BINS - 1];
        int leftCount[NUM_BINS - 1], rightCount[NUM_BINS - 1];
        DirectX::XMFLOAT3 leftBoxMin = { FLT_MAX, FLT_MAX, FLT_MAX }, leftBoxMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        int leftSum = 0;
        for (int i = 0; i < NUM_BINS - 1; ++i)
        {
            leftSum += bins[i].triangleCount; leftCount[i] = leftSum;
            auto min_v = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return DirectX::XMFLOAT3(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)); };
            auto max_v = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return DirectX::XMFLOAT3(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)); };
            leftBoxMin = min_v(leftBoxMin, bins[i].aabbMin); leftBoxMax = max_v(leftBoxMax, bins[i].aabbMax);
            leftArea[i] = SurfaceArea(leftBoxMin, leftBoxMax);
        }
        DirectX::XMFLOAT3 rightBoxMin = { FLT_MAX, FLT_MAX, FLT_MAX }, rightBoxMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        int rightSum = 0;
        for (int i = NUM_BINS - 1; i > 0; --i)
        {
            rightSum += bins[i].triangleCount; rightCount[i - 1] = rightSum;
            auto min_v = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return DirectX::XMFLOAT3(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)); };
            auto max_v = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return DirectX::XMFLOAT3(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)); };
            rightBoxMin = min_v(rightBoxMin, bins[i].aabbMin); rightBoxMax = max_v(rightBoxMax, bins[i].aabbMax);
            rightArea[i - 1] = SurfaceArea(rightBoxMin, rightBoxMax);
        }
        for (int i = 0; i < NUM_BINS - 1; ++i)
        {
            if (leftCount[i] > 0 && rightCount[i] > 0)
            {
                float cost = leftArea[i] * leftCount[i] + rightArea[i] * rightCount[i];
                if (cost < bestCost)
                {
                    bestCost = cost; bestAxis = axis; bestSplitBinIndex = i + 1;
                }
            }
        }
    }

    float parentCost = SurfaceArea(node.aabbMin, node.aabbMax) * count;
    if (bestAxis == -1 || bestCost >= parentCost)
    {
        node.leftChildOrFirstTriangleIndex = baseTriangleIndex + startIndex;
        node.triangleCount = count;
        return;
    }

    float splitPos;
    {
        DirectX::XMFLOAT3 extent = { node.aabbMax.x - node.aabbMin.x, node.aabbMax.y - node.aabbMin.y, node.aabbMax.z - node.aabbMin.z };
        float axisMin, axisExtent;
        if (bestAxis == 0) { axisMin = node.aabbMin.x; axisExtent = extent.x; }
        else if (bestAxis == 1) { axisMin = node.aabbMin.y; axisExtent = extent.y; }
        else { axisMin = node.aabbMin.z; axisExtent = extent.z; }
        splitPos = axisMin + axisExtent * (bestSplitBinIndex / (float)NUM_BINS);
    }
    uint32_t j = startIndex;
    for (uint32_t i = startIndex; i < startIndex + count; ++i)
    {
        float center;
        if (bestAxis == 0) center = (triangles[i].v0.x + triangles[i].v1.x + triangles[i].v2.x) * 0.3333f;
        else if (bestAxis == 1) center = (triangles[i].v0.y + triangles[i].v1.y + triangles[i].v2.y) * 0.3333f;
        else center = (triangles[i].v0.z + triangles[i].v1.z + triangles[i].v2.z) * 0.3333f;
        if (center < splitPos)
        {
            std::swap(triangles[i], triangles[j]);
            j++;
        }
    }
    int leftCount = j - startIndex;
    if (leftCount == 0 || leftCount == count) {
        leftCount = count / 2;
    }


    uint32_t leftChildIndex = atomic_nodesUsed.fetch_add(1);
    uint32_t rightChildIndex = atomic_nodesUsed.fetch_add(1);

    node.leftChildOrFirstTriangleIndex = leftChildIndex;
    node.triangleCount = 0;

    BVHNode& leftChild = bvhNodes[leftChildIndex];
    ComputeAABB(triangles, startIndex, leftCount, leftChild.aabbMin, leftChild.aabbMax);

    BVHNode& rightChild = bvhNodes[rightChildIndex];
    uint32_t rightStartIndex = startIndex + leftCount;
    uint32_t rightCount = count - leftCount;
    ComputeAABB(triangles, rightStartIndex, rightCount, rightChild.aabbMin, rightChild.aabbMax);


    const int PARALLEL_DEPTH_THRESHOLD = 4;

    if (depth < PARALLEL_DEPTH_THRESHOLD)
    {
        auto future = std::async(std::launch::async, [&] {
            SubdivideNode_Parallel(rightChild, triangles, bvhNodes, rightStartIndex, rightCount, baseTriangleIndex, atomic_nodesUsed, depth + 1);
            });

        SubdivideNode_Parallel(leftChild, triangles, bvhNodes, startIndex, leftCount, baseTriangleIndex, atomic_nodesUsed, depth + 1);

        future.get();
    }
    else
    {
        SubdivideNode_Parallel(leftChild, triangles, bvhNodes, startIndex, leftCount, baseTriangleIndex, atomic_nodesUsed, depth + 1);
        SubdivideNode_Parallel(rightChild, triangles, bvhNodes, rightStartIndex, rightCount, baseTriangleIndex, atomic_nodesUsed, depth + 1);
    }
}

void BVHBuilder::Build(std::vector<Triangle>& triangles, std::vector<BVHNode>& outBvhNodes, uint32_t baseTriangleIndex)
{
    outBvhNodes.resize(triangles.size() * 2);

    BVHNode& root = outBvhNodes[0];
    ComputeAABB(triangles, 0, (uint32_t)triangles.size(), root.aabbMin, root.aabbMax);

    uint32_t nodesUsed = 1; // 0 is root
    //std::atomic<uint32_t> nodesUsed(1);

    // Start the recursive subdivision
    SubdivideBinningPartitionNode(root, triangles, outBvhNodes, 0, (uint32_t)triangles.size(), baseTriangleIndex,nodesUsed);

    outBvhNodes.resize(nodesUsed);
}
