#pragma once

#include "RenderEngine Files/global.h"
#include <vector>
#include <string>
#include "CoreHelper Files/Helper.h"

struct Material
{
	XMFLOAT3 Albedo{ 1.0f, 1.0f, 1.0f };
	float Roughness = 1.0f;

	XMFLOAT3 EmissionColor{ 0.0f, 0.0f, 0.0f };
	float EmissionPower = 0.0f;

	XMFLOAT3 AbsorptionColor = { 0.0f, 0.0f, 0.0f };
	float Metallic = 0.0f;
	float Transmission = 0.0f;
	float IOR = 1.5f;


	XMVECTOR GetEmission() const { return XMVectorScale(XMLoadFloat3(&EmissionColor), EmissionPower); }
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