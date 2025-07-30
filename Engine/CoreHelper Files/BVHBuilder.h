#pragma once

#include <vector>
#include "Mesh.h"

namespace BVHBuilder
{

    void Build(std::vector<Triangle>& triangles, std::vector<BVHNode>& outBvhNodes);
}
