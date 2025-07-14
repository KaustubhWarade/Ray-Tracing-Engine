//root Signiture Helper

#include "RootSignitureHelper.h"
#include "D3D.h"
#include "../RenderEngine Files/global.h"

void CreateStaticSampler(
	D3D12_STATIC_SAMPLER_DESC& staticSampler,
	D3D12_FILTER filter,
	D3D12_TEXTURE_ADDRESS_MODE addressMode,
	UINT shaderRegister,
	UINT registerSpace,
	D3D12_SHADER_VISIBILITY shaderVisibility)
{
	staticSampler.Filter = filter;
	staticSampler.AddressU = addressMode;
	staticSampler.AddressV = addressMode;
	staticSampler.AddressW = addressMode;
	staticSampler.MipLODBias = 0;
	staticSampler.MaxAnisotropy = 0;
	staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	staticSampler.MinLOD = 0.0f;
	staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
	staticSampler.ShaderRegister = shaderRegister;
	staticSampler.RegisterSpace = registerSpace;
	staticSampler.ShaderVisibility = shaderVisibility;
}
