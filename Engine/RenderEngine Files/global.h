#pragma once

#include <stdio.h>	 
#include <stdlib.h>
#include <unordered_map>
#include <limits>
#include <filesystem>

#include <d3d12.h>
#include <dxgi1_6.h> //dxgi directx graphics infrastructure
#include <d3dcompiler.h>
#include <DirectXMath.h> //replaces XNAMath
#include <wrl.h>		 // comptr for com object pointer
#include <string>
#include <memory>
#include "dxcapi.h"
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

// Macros
#define WIN_WIDTH 800
#define WIN_HEIGHT 600

extern FILE *gpFile;
extern const char gszLogFileName[];



#ifdef _DEBUG

inline HRESULT LogAndExecute(HRESULT hr, const char* funcName, const char* file, int line) {
    if (gpFile) {
        if (FAILED(hr)) {
            fprintf(gpFile, "FAILED: %s\nFile: %s\nLine: %d\nHRESULT: 0x%08X\n\n",
                funcName, file, line, hr);
        }
        else {
            fprintf(gpFile, "SUCCEEDED: %s\n", funcName);
        }
    }
    return hr;
}

inline HRESULT LogAndExecuteCompile(HRESULT hr, IDxcBlobUtf8* errorBlob, const char* funcName, const char* file, int line)
{
    if (gpFile) {
        if (FAILED(hr)) {
            fprintf(gpFile, "FAILED: %s\nFile: %s\nLine: %d\nHRESULT: 0x%08X\n",
                funcName, file, line, hr);
            if (errorBlob && errorBlob->GetStringLength() > 0) {
                fprintf(gpFile, "Shader Compilation Error:\n%s\n", errorBlob->GetStringPointer());
            }
            fprintf(gpFile, "\n");
        }
        else {
            fprintf(gpFile, "SUCCEEDED: %s\n", funcName);
        }
    }
    return hr;
}

#define EXECUTE_AND_LOG(expr) LogAndExecute((expr), #expr, __FILE__, __LINE__)
#define EXECUTE_AND_LOG_RETURN(expr)                          \
    {                                                         \
        HRESULT _hr = LogAndExecute((expr), #expr, __FILE__, __LINE__); \
        if (FAILED(_hr)) return _hr;                          \
    }
#define COMPILE_AND_LOG_RETURN(expr,errorBlob)                       \
    {                                                                  \
        HRESULT _hr = (expr);                                          \
        LogAndExecuteCompile(_hr, errorBlob, #expr, __FILE__, __LINE__); \
        if (FAILED(_hr)) return _hr;                                   \
    }

#else 

#define EXECUTE_AND_LOG(expr) (expr)
#define EXECUTE_AND_LOG_RETURN(expr)                          \
    {                                                         \
        HRESULT _hr = (expr);                                 \
        if (FAILED(_hr)) return _hr;                          \
    }
#define COMPILE_AND_LOG_RETURN(expr,errorBlob)                \
    {                                                         \
        HRESULT _hr = (expr);                                 \
        if (FAILED(_hr)) return _hr;                          \
    }

#endif 
