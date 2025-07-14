// Shader Loader

#include "ShaderHelper.h"
#include "../RenderEngine Files/global.h"

ComPtr<IDxcUtils> g_dxcUtils;
ComPtr<IDxcCompiler3> g_dxcCompiler;
ComPtr<IDxcIncludeHandler> g_dxcIncludeHandler;

HRESULT InitializeDxcCompiler() // Call this once at application startup
{
    HRESULT hr = S_OK;
    if(!g_dxcUtils || g_dxcCompiler==nullptr)
    {
        EXECUTE_AND_LOG_RETURN(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&g_dxcUtils)));
        EXECUTE_AND_LOG_RETURN(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&g_dxcCompiler)));
        EXECUTE_AND_LOG_RETURN(g_dxcUtils->CreateDefaultIncludeHandler(&g_dxcIncludeHandler));
    }

    return hr;
}

HRESULT CompileShaderDXC(const std::wstring& filename, const wchar_t* entryPoint, const wchar_t* target, Microsoft::WRL::ComPtr<IDxcBlob>& outBlob, ComPtr<IDxcBlobUtf8>& errorBlob)
{

    EXECUTE_AND_LOG_RETURN(InitializeDxcCompiler());
    ComPtr<IDxcBlobEncoding> sourceBlob;
    EXECUTE_AND_LOG_RETURN(g_dxcUtils->LoadFile(filename.c_str(), nullptr, &sourceBlob));

    DxcBuffer sourceBuffer;
    sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
    sourceBuffer.Size = sourceBlob->GetBufferSize();
    sourceBuffer.Encoding = DXC_CP_UTF8;

    std::vector<LPCWSTR> arguments = {
        filename.c_str(),
        L"-E", entryPoint,
        L"-T", target,
#ifdef _DEBUG
        L"-Zi", L"-Qembed_debug", L"-Od"
#else
        L"-O3"
#endif
    };

    ComPtr<IDxcResult> result;
    EXECUTE_AND_LOG_RETURN(g_dxcCompiler->Compile(
        &sourceBuffer,
        arguments.data(), static_cast<UINT32>(arguments.size()),
        g_dxcIncludeHandler.Get(),
        IID_PPV_ARGS(&result)
    ));

    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorBlob), nullptr);
    if (errorBlob && errorBlob->GetStringLength() > 0)
    {
        OutputDebugStringA(errorBlob->GetStringPointer());
    }

    HRESULT compileStatus;
    result->GetStatus(&compileStatus);
    if (FAILED(compileStatus)) return compileStatus;

    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&outBlob), nullptr);
    return S_OK;
}

HRESULT compileVSShader(const std::wstring &filename, SHADER_DATA &shader)
{
//    // variables
//    HRESULT hr = S_OK;
//    Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
//    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
//#ifdef _DEBUG
//    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
//#else
//    UINT compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3; // Full optimization for Release
//#endif
//    // code
//
//    wprintf(L"Compiling shader from: %s\n", filename.c_str());
//    COMPILE_AND_LOG_RETURN(D3DCompileFromFile(
//                               filename.c_str(),
//                               nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
//                               "main", "vs_6_0",
//                               compileFlags, 0,
//                               &shaderBlob,
//                               &errorBlob),
//                           errorBlob.Get());
//
//    shader.vertexBlob = shaderBlob;
//    shader.vertexShaderByteCode.pShaderBytecode = shaderBlob->GetBufferPointer();
//    shader.vertexShaderByteCode.BytecodeLength = shaderBlob->GetBufferSize();


    ComPtr<IDxcBlob> shaderBlob;
    ComPtr<IDxcBlobUtf8> errorBlobs;
    COMPILE_AND_LOG_RETURN(CompileShaderDXC(filename, L"main", L"vs_6_6", shaderBlob, errorBlobs), errorBlobs.Get());

    shader.vertexBlob = shaderBlob;
    shader.vertexShaderByteCode.pShaderBytecode = shaderBlob->GetBufferPointer();
    shader.vertexShaderByteCode.BytecodeLength = shaderBlob->GetBufferSize();
    return S_OK;

}

HRESULT compilePSShader(const std::wstring &filename, SHADER_DATA &shader)
{
//    // variables
//    HRESULT hr = S_OK;
//    Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
//    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
//#ifdef _DEBUG
//    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
//#else
//    UINT compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3; // Full optimization for Release
//#endif
//    // code
//    COMPILE_AND_LOG_RETURN(D3DCompileFromFile(
//                               filename.c_str(),
//                               nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
//                               "main", "ps_6_0",
//                               compileFlags, 0,
//                               &shaderBlob,
//                               &errorBlob),
//                           errorBlob.Get());
//
//    shader.pixelBlob = shaderBlob;
//    shader.pixelShaderByteCode.pShaderBytecode = shaderBlob->GetBufferPointer();
//    shader.pixelShaderByteCode.BytecodeLength = shaderBlob->GetBufferSize();

    ComPtr<IDxcBlob> shaderBlob;
    ComPtr<IDxcBlobUtf8> errorBlobs;
    COMPILE_AND_LOG_RETURN(CompileShaderDXC(filename, L"main", L"ps_6_6", shaderBlob, errorBlobs), errorBlobs.Get());

    shader.pixelBlob = shaderBlob;
    shader.pixelShaderByteCode.pShaderBytecode = shaderBlob->GetBufferPointer();
    shader.pixelShaderByteCode.BytecodeLength = shaderBlob->GetBufferSize();
    return S_OK;
}

HRESULT compileCSShader(const std::wstring& filename, COMPUTE_SHADER_DATA& shader)
{
    //// variables
    //HRESULT hr = S_OK;
    //Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
    //Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    //UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
    //// code
    //COMPILE_AND_LOG_RETURN(D3DCompileFromFile(
    //    filename.c_str(),
    //    nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
    //    "main", "cs_6_0",
    //    compileFlags, 0,
    //    &shaderBlob,
    //    &errorBlob),
    //    errorBlob.Get());

    //shader.computeBlob = shaderBlob;
    //shader.computeShaderByteCode.pShaderBytecode = shaderBlob->GetBufferPointer();
    //shader.computeShaderByteCode.BytecodeLength = shaderBlob->GetBufferSize();

    ComPtr<IDxcBlob> shaderBlob;
    ComPtr<IDxcBlobUtf8> errorBlobs;
    COMPILE_AND_LOG_RETURN(CompileShaderDXC(filename, L"main", L"cs_6_6", shaderBlob, errorBlobs), errorBlobs.Get());

    shader.computeBlob = shaderBlob;
    shader.computeShaderByteCode.pShaderBytecode = shaderBlob->GetBufferPointer();
    shader.computeShaderByteCode.BytecodeLength = shaderBlob->GetBufferSize();
    return S_OK;
}
