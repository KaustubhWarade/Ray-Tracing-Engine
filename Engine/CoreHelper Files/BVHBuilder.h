#pragma once

#include <vector>
#include "Mesh.h"

namespace BVHBuilder
{
    void Build(std::vector<Triangle>& triangles, std::vector<BVHNode>& outBvhNodes, uint32_t baseTriangleIndex);
}
struct Bin
{
    DirectX::XMFLOAT3 aabbMin = { FLT_MAX, FLT_MAX, FLT_MAX };
    DirectX::XMFLOAT3 aabbMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    uint32_t triangleCount = 0;
};
