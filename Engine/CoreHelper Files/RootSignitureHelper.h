#pragma once

#include "../RenderEngine Files/global.h"

void CreateStaticSampler(
	D3D12_STATIC_SAMPLER_DESC& staticSampler,
	D3D12_FILTER filter,
	D3D12_TEXTURE_ADDRESS_MODE addressMode,
	UINT shaderRegister = 0,
	UINT registerSpace = 0,
	D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL);
