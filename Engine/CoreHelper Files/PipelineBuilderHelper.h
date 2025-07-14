#pragma once
#include "../RenderEngine Files/global.h"

#include "ShaderHelper.h"

struct PipeLineStateObject
{
    Microsoft::WRL::ComPtr<ID3D12PipelineState> PipelineState;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
};

void createRasterizerDesc(D3D12_RASTERIZER_DESC &rasterizerDesc);

void createBlendDesc(D3D12_BLEND_DESC &blendDesc);

void createDepthStencilDesc(D3D12_DEPTH_STENCIL_DESC &depthDesc);

void createRenderTargetBlendDesc(D3D12_RENDER_TARGET_BLEND_DESC &rtBlendDesc);

void createPipelineStateDesc(D3D12_GRAPHICS_PIPELINE_STATE_DESC &psoDesc,
                             D3D12_INPUT_ELEMENT_DESC *d3dInputElementDesc,
                             UINT numElements,
                             ID3D12RootSignature *rootSignature,
                             SHADER_DATA shaderData,
                             D3D12_RASTERIZER_DESC &rasterizerDesc,
                             D3D12_BLEND_DESC &blendDesc,
                             D3D12_DEPTH_STENCIL_DESC &depthDesc);