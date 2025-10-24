#pragma once

#include "RenderEngine Files/global.h"
#include <vector>
#include <string>
#include "CoreHelper Files/Helper.h"


struct Sphere
{
	XMFLOAT3 Position{ 0.0f, 0.0f, 0.0f };
	float Radius = 0.5f;

	int MaterialIndex = 0;
};

//struct Triangle
//{
//	XMFLOAT3 v0, v1, v2;    // Vertex positions
//	//XMFLOAT3 n0, n1, n2;    // Vertex normals
//	int MaterialIndex = 0;
//};

struct HitData
{
	float HitDistance;
	XMFLOAT3 HitPosition;
	XMFLOAT3 HitNormal;

	int ObjectIndex;
};


namespace BVH
{
	struct Node
	{
		XMFLOAT3 min_aabb;
		int primitives_count; // Using count instead of indices array for simplicity first. -1 for inner node
		XMFLOAT3 max_aabb;
		int right_child_offset; // Offset to the right child node in the buffer
	};
}
