#pragma once

#include "../RenderEngine Files/global.h"
using namespace DirectX;

// AlphaMode OPAQUE = 0, MASK = 1, BLEND = 2 

struct Material
{
    // PBR Factors
    // ---- Block 1 (16 bytes) ----
    XMFLOAT4 BaseColorFactor{ 1.0f, 1.0f, 1.0f, 1.0f };

    // ---- Block 2 (16 bytes) ----
    XMFLOAT3 EmissiveFactor{ 0.0f, 0.0f, 0.0f };
    float MetallicFactor = 1.0f;

    // ---- Block 3 (16 bytes) ----
    float RoughnessFactor = 1.0f;
    float IOR = 1.5f;
    float Transmission = 0.0f;
    int   AlphaMode = 0; // 0:Opaque, 1:Mask, 2:Blend

    // ---- Block 4 (16 bytes) ----
    float AlphaCutoff = 0.5f;
    BOOL  DoubleSided = false; 
    BOOL  Unlit = false;
    float ClearcoatFactor = 0.0f;

    // ---- Block 5 (16 bytes) ----
    float ClearcoatRoughnessFactor = 0.0f;
    XMFLOAT3 SheenColorFactor{ 0.0f, 0.0f, 0.0f };

    // ---- Block 6 (16 bytes) ----
    float SheenRoughnessFactor = 0.0f;
    int   BaseColorTextureIndex = -1;
    int   MetallicRoughnessTextureIndex = -1;
    int   NormalTextureIndex = -1;

    // ---- Block 7 (16 bytes) ----
    int   OcclusionTextureIndex = -1;
    int   EmissiveTextureIndex = -1;
    int   ClearcoatTextureIndex = -1;
    int   ClearcoatRoughnessTextureIndex = -1;

    // ---- Block 8 (16 bytes) ----
    int   ClearcoatNormalTextureIndex = -1;
    int   SheenColorTextureIndex = -1;
    int   SheenRoughnessTextureIndex = -1;
    int   TransmissionTextureIndex = -1;
};