#pragma once
#include "../RenderEngine Files/global.h"

struct Triangle
{
    DirectX::XMFLOAT3 v0, v1, v2; // Vertex positions
    int MaterialIndex;
    DirectX::XMFLOAT3 n0, n1, n2; // Vertex normals
};

struct BVHNode
{
    DirectX::XMFLOAT3 aabbMin;
    int leftChildOrFirstTriangleIndex;
    DirectX::XMFLOAT3 aabbMax;
    int triangleCount;
    // If triangleCount > 0 (leaf): index of the first triangle.
    // If triangleCount == 0 (internal): index of the left child node.
};