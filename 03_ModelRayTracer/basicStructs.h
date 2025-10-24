#pragma once

#include "RenderEngine Files/global.h"
#include <vector>
#include <string>
#include "CoreHelper Files/Helper.h"

struct MaterialGPUData
{
    std::string name;
    // PBR Factors
    XMFLOAT4 BaseColorFactor{ 1.0f, 1.0f, 1.0f, 1.0f };
    XMFLOAT3 EmissiveFactor{ 0.0f, 0.0f, 0.0f };
    float MetallicFactor = 0.0f;

    float RoughnessFactor = 1.0f;
    float IOR = 1.5f;           
    float Transmission = 0.0f;  
    float _padding0;            

    // Bindless Texture Indices 
    int BaseColorTextureIndex = -1;
    int MetallicRoughnessTextureIndex = -1;
    int NormalTextureIndex = -1;
    int OcclusionTextureIndex = -1; 
};

struct Sphere
{
	XMFLOAT3 Position{ 0.0f, 0.0f, 0.0f };
	float Radius = 0.5f;

	int MaterialIndex = 0;
};

struct HitData
{
	float HitDistance;
	XMFLOAT3 HitPosition;
	XMFLOAT3 HitNormal;

	int ObjectIndex;
};