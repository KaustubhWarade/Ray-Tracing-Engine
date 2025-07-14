#pragma once
#include "../RenderEngine Files/global.h"
#include "dxcapi.h"
#pragma comment(lib, "dxcompiler.lib")

struct SHADER_DATA
{
    Microsoft::WRL::ComPtr<IDxcBlob> vertexBlob;
    Microsoft::WRL::ComPtr<IDxcBlob> pixelBlob;
    D3D12_SHADER_BYTECODE vertexShaderByteCode = {};
    D3D12_SHADER_BYTECODE pixelShaderByteCode = {};
};
struct COMPUTE_SHADER_DATA
{
    Microsoft::WRL::ComPtr<IDxcBlob> computeBlob;
    D3D12_SHADER_BYTECODE computeShaderByteCode = {};
};
HRESULT InitializeDxcCompiler();
HRESULT compileVSShader(const std::wstring &filename, SHADER_DATA &shaderByteCode);
HRESULT compilePSShader(const std::wstring &filename, SHADER_DATA &shaderByteCode);

HRESULT compileCSShader(const std::wstring& filename, COMPUTE_SHADER_DATA& shaderByteCode);
