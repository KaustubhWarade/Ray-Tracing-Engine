#include "PipelineBuilderHelper.h"
#include "ShaderHelper.h"
#include "../RenderEngine Files/global.h"
using namespace DirectX;

void createRasterizerDesc(D3D12_RASTERIZER_DESC &rasterizerDesc)
{
    // code
    ZeroMemory(&rasterizerDesc, sizeof(D3D12_RASTERIZER_DESC));
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE::D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
}

void createBlendDesc(D3D12_BLEND_DESC &blendDesc)
{
    // code
    ZeroMemory(&blendDesc, sizeof(D3D12_BLEND_DESC));
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;

    D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc;
    ZeroMemory(&rtBlendDesc, sizeof(D3D12_RENDER_TARGET_BLEND_DESC));
    rtBlendDesc.BlendEnable = FALSE;
    rtBlendDesc.LogicOpEnable = FALSE;
    rtBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    rtBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    blendDesc.RenderTarget[0] = rtBlendDesc; // Apply to the first render target
}

void createRenderTargetBlendDesc(D3D12_RENDER_TARGET_BLEND_DESC &rtBlendDesc)
{
    // code
    ZeroMemory(&rtBlendDesc, sizeof(D3D12_RENDER_TARGET_BLEND_DESC));
    rtBlendDesc.BlendEnable = FALSE;
    rtBlendDesc.LogicOpEnable = FALSE;
    rtBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    rtBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // Write all channels
}

void createDepthStencilDesc(D3D12_DEPTH_STENCIL_DESC &depthDesc)
{
    // code
    ZeroMemory(&depthDesc, sizeof(D3D12_DEPTH_STENCIL_DESC));
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depthDesc.StencilEnable = FALSE;
    depthDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    depthDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
        {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS};
    depthDesc.FrontFace = defaultStencilOp;
    depthDesc.BackFace = defaultStencilOp;
}

void createPipelineStateDesc(D3D12_GRAPHICS_PIPELINE_STATE_DESC &psoDesc,
                             D3D12_INPUT_ELEMENT_DESC *d3dInputElementDesc,
                             UINT numElements,
                             ID3D12RootSignature *rootSignature,
                             SHADER_DATA shaderData,
                             D3D12_RASTERIZER_DESC &rasterizerDesc,
                             D3D12_BLEND_DESC &blendDesc,
                             D3D12_DEPTH_STENCIL_DESC &depthDesc)
{
    // code
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = {d3dInputElementDesc, numElements};
    psoDesc.pRootSignature = rootSignature;
    psoDesc.VS = shaderData.vertexShaderByteCode;
    psoDesc.PS = shaderData.pixelShaderByteCode;
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState = depthDesc;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count = 1;
}
