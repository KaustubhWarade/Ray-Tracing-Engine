#pragma once
#include "../RenderEngine Files/global.h"
#include <random>


class Random
{
public:
	Random()
	{
		s_RandomEngine.seed(std::random_device()());
	}

	static uint32_t UInt()
	{
		return s_Distribution(s_RandomEngine);
	}

	static uint32_t UInt(uint32_t min, uint32_t max)
	{
		return min + (s_Distribution(s_RandomEngine) % (max - min + 1));
	}

	static float Float()
	{
		return (float)s_Distribution(s_RandomEngine) / (float)UINT32_MAX;
	}

	static DirectX::XMFLOAT3 Float3()
	{
		return DirectX::XMFLOAT3(Float(), Float(), Float());
	}

	static DirectX::XMFLOAT3 Float3(float min, float max)
	{
		return DirectX::XMFLOAT3(Float() * (max - min) + min, Float() * (max - min) + min, Float() * (max - min) + min);
	}

	static DirectX::XMVECTOR Vector3()
	{
		DirectX::XMVECTOR vec = DirectX::XMVectorSet(Float(),Float(),Float(), 0.0f);
		return vec;
	}

	static DirectX::XMVECTOR Vector3(float min, float max)
	{
		DirectX::XMVECTOR vec = DirectX::XMVectorSet(Float() * (max - min) + min, Float() * (max - min) + min, Float() * (max - min) + min,1.0f);
		return vec;
	}

	static DirectX::XMVECTOR InUnitSphere()
	{
		DirectX::XMVECTOR temp = DirectX::XMVector3Normalize(Vector3());
		return temp;
	}

private:
	static thread_local std::mt19937 s_RandomEngine;
	static std::uniform_int_distribution<uint32_t> s_Distribution;

};
